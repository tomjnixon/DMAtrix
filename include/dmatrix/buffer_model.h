#pragma once

#include <algorithm>
#include <vector>
#include "display_model.h"

namespace DMAtrix {

  template <typename D>
  struct BufferModel {
    size_t num_bits;

    struct SubFrame {
      size_t bit;
      size_t addr;
      size_t oe_length;
      size_t data_offset = 0;
      size_t oe_offset = 0;
      size_t addr_transition = 0;
      // le is always enabled the cycle after the data has loaded
    };

    std::vector<SubFrame> subframes;

    void allocate_subframes(size_t min_pulse) {
      for (size_t i = 0; i < num_bits; i++) {
        // interleave the high and low bits to distribute the gaps more evenly
        size_t bit = (i & 1) == 0 ? i : (num_bits & ~1) - i;
        for (size_t addr = 0; addr < (1 << D::addr_bits); addr++)
          subframes.push_back({bit, addr, min_pulse << bit});
      }
    }

    void pack_subframes() {
      for (size_t i = 0; i < subframes.size(); i++) {
        SubFrame &frame = subframes[i];
        SubFrame &next_frame = subframes[(i + 1) % subframes.size()];

        size_t data_end = frame.data_offset + D::data_words;
        frame.oe_offset = data_end + 1;
        size_t oe_end = frame.oe_offset + frame.oe_length;

        next_frame.data_offset = std::max(data_end, oe_end - D::data_words);
      }

      // above procedure calculates the data offset the first subframe as being
      // after the last subframe, but in fact it starts at 0; this is therefore
      // the complete buffer length
      buf_len = subframes[0].data_offset;
      subframes[0].data_offset = 0;
    }

    void calc_addr_transitions() {
      for (size_t i = 0; i < subframes.size(); i++) {
        SubFrame &frame_a = subframes[i];
        SubFrame &frame_b = subframes[(i + 1) % subframes.size()];

        size_t oe_end_a = frame_a.oe_offset + frame_a.oe_length;
        size_t oe_start_b = frame_b.oe_offset;

        if (oe_start_b < oe_end_a) oe_start_b += buf_len;

        frame_b.addr_transition = (oe_end_a + oe_start_b) / 2;
        frame_b.addr_transition %= buf_len;
      }
    }

    std::vector<size_t> data_offsets;
    size_t &data_offset(size_t bit, size_t addr) {
      return data_offsets[(bit << D::addr_bits) + addr];
    }

    void fill_data_offsets() {
      data_offsets.resize(subframes.size());
      for (auto &frame : subframes) {
        data_offset(frame.bit, frame.addr) = frame.data_offset;
      }
    }

    BufferModel(size_t min_pulse, size_t num_bits) : num_bits(num_bits) {
      allocate_subframes(min_pulse);
      pack_subframes();
      calc_addr_transitions();
      fill_data_offsets();
    }

    size_t buf_len;

    size_t buf_idx(size_t bit, size_t addr, size_t word) {
      return data_offset(bit, addr) + word;
    }

    static constexpr int oe_bit() { return 0; }
    static constexpr int le_bit() { return 1; }
    static constexpr int addr_bit(size_t bit) { return 2 + bit; }
    static constexpr int addr_enc(size_t addr) { return addr << 2; }
    static constexpr int data_bit(size_t bit) { return 2 + D::addr_bits + bit; }

    template <typename Buffer>
    void init_buffer(Buffer &buf) {
      for (size_t i = 0; i < buf_len; i++) buf[i] = 1 << oe_bit();

      for (size_t i = 0; i < subframes.size(); i++) {
        buf[(subframes[i].data_offset + D::data_words) % buf_len] |=
            1 << le_bit();

        for (size_t j = 0; j < subframes[i].oe_length; j++) {
          buf[(subframes[i].oe_offset + j) % buf_len] &= ~(1 << oe_bit());
        }
      }

      for (size_t i = 0; i < subframes.size(); i++) {
        SubFrame &frame_a = subframes[i];
        SubFrame &frame_b = subframes[(i + 1) % subframes.size()];
        size_t addr_start = frame_a.addr_transition;
        size_t addr_end = frame_b.addr_transition;
        if (addr_end < addr_start) addr_end += buf_len;

        for (size_t j = addr_start; j < addr_end; j++)
          buf[j % buf_len] |= addr_enc(frame_a.addr);
      }
    }

    template <typename T, size_t num_bits_value, typename Buffer>
    void write_color(Buffer &buf, size_t row, size_t col, size_t color,
                     T value) {
      DataAddr addr = D::encode(row, col, color);

      for (size_t bit = 0; bit < num_bits; bit++) {
        int value_bit = (int)bit + (num_bits_value - num_bits);

        if (value_bit >= 0 && ((value >> value_bit) & 1))
          buf[buf_idx(bit, addr.addr, D::data_words - addr.word)] |=
              1 << data_bit(addr.bit);
        else
          buf[buf_idx(bit, addr.addr, D::data_words - addr.word)] &=
              ~(1 << data_bit(addr.bit));
      }
    }

    template <typename T, size_t num_bits_value, typename Buffer>
    void write_rgb(Buffer &buf, size_t row, size_t col, T r, T g, T b) {
      write_color<T, num_bits_value>(buf, row, col, 0, r);
      write_color<T, num_bits_value>(buf, row, col, 1, g);
      write_color<T, num_bits_value>(buf, row, col, 2, b);
    }
  };

}
