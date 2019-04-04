#pragma once

#include <array>
#include <type_traits>
#include "buffer_model.h"

namespace DMAtrix {

  template <typename Display, template <size_t, size_t> typename PinDriver,
            bool double_buffered>
  struct DisplayDriver {
    using PinsT = Pins<Display>;
    static constexpr size_t num_buffers = double_buffered ? 2 : 1;
    size_t back_buffer = double_buffered ? 1 : 0;

    using PinDriverT = PinDriver<PinsT::num_bits, num_buffers>;
    using DriverConfig = typename PinDriverT::Config;
    PinDriverT pin_driver;

    BufferModel<Display> buffer_model;

    DisplayDriver(PinsT pins, size_t min_pulse, size_t num_bits,
                  DriverConfig driver_config = {})
        : buffer_model(min_pulse, num_bits) {
      std::array<int, PinsT::num_bits> data_pins;
      data_pins[buffer_model.oe_bit()] = pins.oe;
      data_pins[buffer_model.le_bit()] = pins.le;

      for (size_t i = 0; i < Display::addr_bits; i++)
        data_pins[buffer_model.addr_bit(i)] = pins.addr[i];
      for (size_t i = 0; i < Display::data_bits; i++)
        data_pins[buffer_model.data_bit(i)] = pins.data[i];

      pin_driver.setup(data_pins, pins.clk, driver_config,
                       buffer_model.buf_len);

      for (size_t i = 0; i < num_buffers; i++)
        buffer_model.init_buffer(pin_driver.buffers[i]);
    }

    template <typename T = uint8_t, int num_bits_value = 8>
    void write_rgb(size_t row, size_t col, T r, T g, T b) {
      buffer_model.template write_rgb<T, num_bits_value>(
          pin_driver.buffers[back_buffer], row, col, r, g, b);
    }

    void flip() {
      if (double_buffered) {
        pin_driver.flip_to(back_buffer);
        back_buffer ^= 1;
      }
    }

    bool flip_done() { return double_buffered ? pin_driver.flip_done() : true; }
  };

}
