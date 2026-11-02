#include "gc9b72_spi.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "gc9b72_init_ops.h"

namespace {
constexpr gpio_num_t kSck = GPIO_NUM_18;
constexpr gpio_num_t kMosi = GPIO_NUM_23;
constexpr gpio_num_t kCs = GPIO_NUM_13;
constexpr gpio_num_t kDc = GPIO_NUM_14;
constexpr gpio_num_t kReset = GPIO_NUM_27;
constexpr gpio_num_t kBacklight = GPIO_NUM_26;
constexpr int kPhysical = 360;
constexpr int kLogical = 466;
constexpr size_t kLogicalBufferPixels = kLogical * 12;

spi_device_handle_t s_spi = nullptr;
lv_display_t *s_display = nullptr;
alignas(4) lv_color_t s_drawBuffer[kLogicalBufferPixels];
alignas(4) uint8_t s_txRow[kPhysical * 2];

esp_err_t send_bytes(bool data, const uint8_t *bytes, size_t length) {
  if (!length) return ESP_OK;
  gpio_set_level(kDc, data ? 1 : 0);
  spi_transaction_t transaction{};
  transaction.length = length * 8;
  transaction.tx_buffer = bytes;
  return spi_device_transmit(s_spi, &transaction);
}

esp_err_t command(uint8_t cmd, const uint8_t *data = nullptr, size_t length = 0) {
  const esp_err_t result = send_bytes(false, &cmd, 1);
  if (result != ESP_OK) return result;
  if (length) return send_bytes(true, data, length);
  return ESP_OK;
}

void run_init_table() {
  const uint8_t *p = gc9b72_init_operations;
  const uint8_t *end = p + sizeof(gc9b72_init_operations);
  while (p < end) {
    const uint8_t op = *p++;
    switch (op) {
      case BEGIN_WRITE:
      case END_WRITE:
        break;
      case WRITE_COMMAND_8:
        command(*p++);
        break;
      case WRITE_COMMAND_16: {
        const uint8_t cmd[] = {p[1], p[0]}; p += 2;
        send_bytes(false, cmd, sizeof(cmd));
        break;
      }
      case WRITE_COMMAND_BYTES: {
        const size_t n = *p++;
        send_bytes(false, p, n); p += n;
        break;
      }
      case WRITE_DATA_8:
        send_bytes(true, p++, 1);
        break;
      case WRITE_DATA_16:
        send_bytes(true, p, 2); p += 2;
        break;
      case WRITE_BYTES: {
        const size_t n = *p++;
        send_bytes(true, p, n); p += n;
        break;
      }
      case WRITE_C8_D8: {
        const uint8_t cmd = *p++, data = *p++;
        command(cmd, &data, 1);
        break;
      }
      case WRITE_C8_D16: {
        const uint8_t cmd = *p++;
        const uint8_t data[] = {p[1], p[0]}; p += 2;
        command(cmd, data, sizeof(data));
        break;
      }
      case WRITE_C8_BYTES: {
        const uint8_t cmd = *p++;
        const size_t n = *p++;
        command(cmd, p, n); p += n;
        break;
      }
      case WRITE_C16_D16: {
        const uint8_t cmd[] = {p[1], p[0]}; p += 2;
        const uint8_t data[] = {p[1], p[0]}; p += 2;
        send_bytes(false, cmd, sizeof(cmd));
        send_bytes(true, data, sizeof(data));
        break;
      }
      case DELAY:
        vTaskDelay(pdMS_TO_TICKS(*p++));
        break;
      default:
        return;
    }
  }
}

void set_window(int x0, int y0, int x1, int y1) {
  const uint8_t col[] = {static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0),
                         static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1)};
  const uint8_t row[] = {static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0),
                         static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1)};
  command(0x2A, col, sizeof(col));
  command(0x2B, row, sizeof(row));
  const uint8_t ramWrite = 0x2C;
  send_bytes(false, &ramWrite, 1);
}

void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pxMap) {
  const int srcW = area->x2 - area->x1 + 1;
  const int x0 = static_cast<int>(std::clamp<int64_t>((static_cast<int64_t>(area->x1) * kPhysical) / kLogical, 0, kPhysical - 1));
  const int y0 = static_cast<int>(std::clamp<int64_t>((static_cast<int64_t>(area->y1) * kPhysical) / kLogical, 0, kPhysical - 1));
  const int x1 = static_cast<int>(std::clamp<int64_t>(((static_cast<int64_t>(area->x2) + 1) * kPhysical + kLogical - 1) / kLogical - 1, 0, kPhysical - 1));
  const int y1 = static_cast<int>(std::clamp<int64_t>(((static_cast<int64_t>(area->y2) + 1) * kPhysical + kLogical - 1) / kLogical - 1, 0, kPhysical - 1));
  auto *source = reinterpret_cast<lv_color_t *>(pxMap);
  set_window(x0, y0, x1, y1);
  for (int py = y0; py <= y1; ++py) {
    const int sy = static_cast<int>(std::clamp<int32_t>((py * kLogical) / kPhysical, area->y1, area->y2) - area->y1);
    for (int px = x0; px <= x1; ++px) {
      const int sx = static_cast<int>(std::clamp<int32_t>((px * kLogical) / kPhysical, area->x1, area->x2) - area->x1);
      const uint16_t color = lv_color_to_u16(source[sy * srcW + sx]);
      const size_t offset = static_cast<size_t>(px - x0) * 2;
      s_txRow[offset] = static_cast<uint8_t>(color >> 8);
      s_txRow[offset + 1] = static_cast<uint8_t>(color);
    }
    gpio_set_level(kDc, 1);
    spi_transaction_t transaction{};
    transaction.length = static_cast<size_t>(x1 - x0 + 1) * 16;
    transaction.tx_buffer = s_txRow;
    spi_device_transmit(s_spi, &transaction);
  }
  lv_display_flush_ready(display);
}
}  // namespace

namespace gc9b72_spi {
bool init() {
  gpio_config_t outputs{};
  outputs.pin_bit_mask = (1ULL << kCs) | (1ULL << kDc) | (1ULL << kReset) | (1ULL << kBacklight);
  outputs.mode = GPIO_MODE_OUTPUT;
  outputs.pull_up_en = GPIO_PULLUP_DISABLE;
  outputs.pull_down_en = GPIO_PULLDOWN_DISABLE;
  outputs.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&outputs) != ESP_OK) return false;
  gpio_set_level(kCs, 1); gpio_set_level(kDc, 1); gpio_set_level(kBacklight, 0);
  gpio_set_level(kReset, 0); vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(kReset, 1); vTaskDelay(pdMS_TO_TICKS(120));

  spi_bus_config_t bus{};
  bus.sclk_io_num = kSck; bus.mosi_io_num = kMosi; bus.miso_io_num = -1;
  bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = kPhysical * 2;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  spi_device_interface_config_t device{};
  device.clock_speed_hz = 40 * 1000 * 1000; device.mode = 0; device.spics_io_num = kCs;
  device.queue_size = 1; device.flags = SPI_DEVICE_HALFDUPLEX;
  if (spi_bus_add_device(SPI2_HOST, &device, &s_spi) != ESP_OK) return false;
  run_init_table();
  gpio_set_level(kBacklight, 1);

  s_display = lv_display_create(kLogical, kLogical);
  if (!s_display) return false;
  lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s_display, flush);
  lv_display_set_buffers(s_display, s_drawBuffer, nullptr, sizeof(s_drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  return true;
}
}  // namespace gc9b72_spi
