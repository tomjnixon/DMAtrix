#include <Arduino.h>
#include <dmatrix/display_model.h>
#include <dmatrix/driver.h>
#include <dmatrix/hw/esp32.h>

using namespace DMAtrix;

using D = FullDisplay<32, 64, 4, RGBOrder::RRGGBB>;
// clk, oe, le, {A, B, C, D}, {R1, R2, G1, G2, B1, B2}
Pins<D> pins{23, 22, 4, {13, 12, 14, 27}, {21, 19, 18, 5, 17, 16}};
DisplayDriver<D, ESP32I2SDMA, true> driver(pins, 4, 8);

long unsigned int last_print_time;
size_t count = 0;

void setup() {
  Serial.begin(115200);

  // draw a static image onto both buffers
  for (int i = 0; i < 2; i++) {
    driver.write_rgb(0, 0, 255, 255, 255);
    for (int x = 0; x < 256; x++) driver.write_rgb(x / 8, 8 + x % 8, x, 0, 0);
    for (int x = 0; x < 256; x++) driver.write_rgb(x / 8, 24 + x % 8, 0, x, 0);
    for (int x = 0; x < 256; x++) driver.write_rgb(x / 8, 40 + x % 8, 0, 0, x);
    driver.flip();
  }

  last_print_time = micros();
}

void loop() {
  // draw an animation
  long int offset_r = millis() / 13;
  long int offset_g = millis() / 17;
  long int offset_b = millis() / 19;
  for (int x = 0; x < 256; x++) {
    int r = ((offset_r + x) % 255) - 127;
    int g = ((offset_g + x) % 255) - 127;
    int b = ((offset_b + x) % 255) - 127;
    driver.write_rgb(x / 8, 56 + x % 8, (r * r) >> 6, (g * g) >> 6,
                     (b * b) >> 6);
  }

  // flip to the other buffer, and wait for the switch to happen
  driver.flip();
  while (!driver.flip_done())
    ;

  // print an fps counter
  count++;
  if (micros() > last_print_time + 1000000) {
    Serial.print(count);
    Serial.println(" fps");
    count = 0;
    last_print_time += 1000000;
  }
}
