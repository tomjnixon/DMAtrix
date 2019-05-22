#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace DMAtrix {

  struct DataAddr {
    size_t addr;
    size_t bit;
    size_t word;
  };

  enum class RGBOrder { RGBRGB, RRGGBB };

  template <size_t _rows, size_t _cols, size_t _addr_bits,
            RGBOrder rgb_order = RGBOrder::RGBRGB>
  struct FullDisplay {
    static constexpr size_t rows = _rows, cols = _cols;
    static constexpr size_t addr_bits = _addr_bits;
    static constexpr size_t colors = 3;
    static constexpr size_t data_bits = 3 * rows >> addr_bits;
    static constexpr size_t data_words = cols;

    static constexpr DataAddr encode(size_t row, size_t col, size_t color) {
      size_t bit = 0;
      switch (rgb_order) {
        case RGBOrder::RGBRGB:
          bit = 3 * (row >> addr_bits) + color;
          break;
        case RGBOrder::RRGGBB:
          bit = (color * (rows >> addr_bits)) + (row >> addr_bits);
          break;
      }

      size_t addr = row & ((1 << addr_bits) - 1);

      return DataAddr{addr, bit, col};
    }
  };

  template <typename FD>
  struct WrappedDisplay : FD {
    static constexpr size_t data_bits = 1;
    static constexpr size_t data_words = FD::data_words * FD::data_bits;

    static constexpr DataAddr encode(size_t row, size_t col, size_t color) {
      DataAddr full_addr = FD::encode(row, col, color);
      return DataAddr{full_addr.addr, 0,
                      full_addr.bit * FD::data_words + full_addr.word};
    }
  };

  template <typename D>
  struct Pins {
    size_t clk;
    size_t oe;
    size_t le;
    size_t addr[D::addr_bits];
    size_t data[D::data_bits];

    static constexpr size_t num_bits = 2 + D::addr_bits + D::data_bits;
    static constexpr size_t num_pins = num_bits + 1;
  };

}
