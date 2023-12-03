#include <dmatrix/hw/esp32.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace DMAtrix;

// output should look like this:
//               __    __    __    __    __
// clk (18)   __|  |__|  |__|  |__|  |__|  |
//                  _____       _____
// data (19)  _____|     |_____|     |_____
//
// with the right-most data pulse moving right one clock per second, looping
// every 10 seconds

constexpr size_t bits = 8;
size_t test_bit = 0;

ESP32I2SDMA<bits, 1> dma;

extern "C" {
void app_main() {
  size_t buf_len = 1000;

  ESP32Config config;
  config.clkspeed_hz = 1000000;
  config.dev = 1;

  std::array<int, bits> pins{-1};
  pins[test_bit] = 19;

  dma.setup(pins, 18, config, buf_len);

  for (size_t i = 0; i < buf_len; i++) dma.buffers[0][i] = 0;
  dma.buffers[0][buf_len - 2] = 1 << test_bit;

  while (1) {
    for (size_t i = 0; i < 10; i++) {
      dma.buffers[0][i] = 1 << test_bit;
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      dma.buffers[0][i] = 0;
    }
  }
}
}
