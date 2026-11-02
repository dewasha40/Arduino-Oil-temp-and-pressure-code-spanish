#include "gc9b72_spi.h"
#include "hella_reader.hpp"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>

namespace {
lv_obj_t *screen,*oilValue,*barValue,*batteryValue,*oilMax,*barMax,*status;
float peakOil=-40,peakPressure=0;
lv_color_t mint=lv_color_hex(0x39e49b),grey=lv_color_hex(0x929aa3);
lv_obj_t* text(const char *s,int size,lv_color_t c,int y){auto*l=lv_label_create(screen);lv_label_set_text(l,s);lv_obj_set_style_text_color(l,c,0);lv_obj_set_style_text_font(l,size>=40?&lv_font_montserrat_40:size>=32?&lv_font_montserrat_32:size>=24?&lv_font_montserrat_24:size>=18?&lv_font_montserrat_18:&lv_font_montserrat_14,0);lv_obj_set_width(l,320);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(l,LV_ALIGN_CENTER,0,y);return l;}
void ui_task(void*){int64_t last=esp_timer_get_time();char b[64];for(;;){int64_t now=esp_timer_get_time();lv_tick_inc(static_cast<uint32_t>((now-last)/1000));last=now;auto r=hella::read();if(r.oil_valid){peakOil=std::max(peakOil,r.oil_c);std::snprintf(b,sizeof(b),"%.0f °C",r.oil_c);lv_label_set_text(oilValue,b);std::snprintf(b,sizeof(b),"MAX ACEITE  %.0f °C",peakOil);lv_label_set_text(oilMax,b);}if(r.pressure_valid){peakPressure=std::max(peakPressure,r.pressure_bar);std::snprintf(b,sizeof(b),"%.1f bar",r.pressure_bar);lv_label_set_text(barValue,b);std::snprintf(b,sizeof(b),"MAX PRESION  %.1f bar",peakPressure);lv_label_set_text(barMax,b);}std::snprintf(b,sizeof(b),"%.1f V",r.battery_v);lv_label_set_text(batteryValue,b);lv_label_set_text(status,(r.oil_valid||r.pressure_valid)?"SENSOR HELLA CONECTADO":"ESPERANDO SENSOR PWM");lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(50));}}
}
extern "C" void app_main(){lv_init();if(!gc9b72_spi::init()||!hella::begin())return;screen=lv_obj_create(nullptr);lv_obj_remove_style_all(screen);lv_obj_set_size(screen,466,466);lv_obj_set_style_bg_color(screen,lv_color_black(),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);auto*ring=lv_obj_create(screen);lv_obj_remove_style_all(ring);lv_obj_set_size(ring,438,438);lv_obj_center(ring);lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(ring,2,0);lv_obj_set_style_border_color(ring,lv_color_hex(0x34464d),0);text("MEDIDOR HELLA",20,mint,-180);text("TEMPERATURA DE ACEITE",16,grey,-115);oilValue=text("-- °C",40,lv_color_white(),-70);text("PRESION DE ACEITE",16,grey,-18);barValue=text("--.- bar",28,lv_color_white(),20);oilMax=text("MAX ACEITE  -- °C",16,mint,75);barMax=text("MAX PRESION  -- bar",16,mint,105);batteryValue=text("--.- V",20,lv_color_white(),151);status=text("ESPERANDO SENSOR PWM",14,grey,186);lv_screen_load(screen);xTaskCreate(ui_task,"ui",6144,nullptr,5,nullptr);}
