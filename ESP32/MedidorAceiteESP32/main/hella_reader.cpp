#include "hella_reader.hpp"
#include <algorithm>
#include <cmath>
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace {
constexpr gpio_num_t kHellaInput=GPIO_NUM_34;
constexpr adc1_channel_t kBatteryAdc=ADC1_CHANNEL_7; // GPIO35
constexpr float kR1=27000.0f,kR2=4700.0f,kPressureCorrectionBar=0.88f;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t lowUs[3]{},highUs[3]{};
volatile bool fresh[3]{};
volatile int channel=0,lastLevel=1;
volatile int64_t edgeUs=0,lastRiseUs=0;
void IRAM_ATTR edge_isr(void*) {
  portENTER_CRITICAL_ISR(&mux);
  const int64_t now=esp_timer_get_time(); const int level=gpio_get_level(kHellaInput);
  if(level==lastLevel){portEXIT_CRITICAL_ISR(&mux);return;}
  if(level){
    const uint32_t period=static_cast<uint32_t>(now-lastRiseUs);lowUs[channel]=period;
    if(period<3000)channel=0;else if(channel<2)++channel;else channel=0;
    lastRiseUs=now;
  }else{
    const uint32_t high=static_cast<uint32_t>(now-lastRiseUs);highUs[channel]=high;fresh[channel]=true;
  }
  edgeUs=now;lastLevel=level;portEXIT_CRITICAL_ISR(&mux);
}
}
namespace hella {
bool begin(){gpio_config_t io{};io.pin_bit_mask=1ULL<<kHellaInput;io.mode=GPIO_MODE_INPUT;io.pull_up_en=GPIO_PULLUP_DISABLE;io.pull_down_en=GPIO_PULLDOWN_DISABLE;io.intr_type=GPIO_INTR_ANYEDGE;if(gpio_config(&io)!=ESP_OK)return false;if(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)!=ESP_OK)return false;edgeUs=lastRiseUs=esp_timer_get_time();lastLevel=gpio_get_level(kHellaInput);if(gpio_isr_handler_add(kHellaInput,edge_isr,nullptr)!=ESP_OK)return false;adc1_config_width(ADC_WIDTH_BIT_12);adc1_config_channel_atten(kBatteryAdc,ADC_ATTEN_DB_11);return true;}
Reading read(){uint32_t lows[3]{},highs[3]{};bool valid[3]{};portENTER_CRITICAL(&mux);for(int i=0;i<3;i++){lows[i]=lowUs[i];highs[i]=highUs[i];valid[i]=fresh[i];fresh[i]=false;}portEXIT_CRITICAL(&mux);Reading r{};if(valid[0]&&lows[0]>0&&highs[0]>0){r.oil_c=((4096.0f/lows[0])*highs[0]-128.0f)/19.2f-40.0f;r.oil_valid=std::isfinite(r.oil_c)&&r.oil_c>-40&&r.oil_c<250;}if(valid[1]&&lows[1]>0&&highs[1]>0){r.pressure_bar=std::max(0.0f,(((4096.0f/lows[1])*highs[1]-128.0f)/384.0f+0.5f)-kPressureCorrectionBar);r.pressure_valid=std::isfinite(r.pressure_bar)&&r.pressure_bar<20;}if(valid[2]&&lows[2]>0&&highs[2]>0){r.auxiliary=(1024.0f/lows[2])*highs[2];if(r.auxiliary>=231&&r.auxiliary<=281)r.auxiliary=1;r.auxiliary_valid=std::isfinite(r.auxiliary);}const int raw=adc1_get_raw(kBatteryAdc);const float pin=static_cast<float>(raw)*3.3f/4095.0f;r.battery_v=std::clamp((pin-0.10f)*((kR1+kR2)/kR2),0.0f,18.0f);return r;}
}
