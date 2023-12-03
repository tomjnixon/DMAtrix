#include <dmatrix/buffer_model.h>
#include <dmatrix/display_model.h>
#include <dmatrix/driver.h>

#include <Eigen/Core>
#include <unsupported/Eigen/CXX11/Tensor>

#include "catch.hpp"

using namespace DMAtrix;

using Image = Eigen::Tensor<unsigned int, 3>;

template <size_t num_pins, size_t num_buffers>
struct DummyDriver {
  using dtype = uint32_t;
  std::array<std::vector<dtype>, num_buffers> buffers;

  struct Config {};

  void setup(std::array<int, num_pins> data_pins, int clk_pin, Config config,
             size_t size) {
    for (auto &buffer : buffers) buffer.resize(size);
  }

  /// decode the image in buffer[buf]
  template <typename D>
  Image decode(size_t buf) {
    Image res((int)D::rows, (int)D::cols, (int)D::colors);
    res.setZero();

    // state of shift register on LE
    std::vector<unsigned int> shift_reg_front(D::data_words);
    // circular buffer with for the non-visible shift register
    std::vector<unsigned int> shift_reg_back(D::data_words);
    size_t shift_reg_ptr = 0;

    unsigned int oe_clocks = 0;
    unsigned int oe_addr = 0;

    for (auto &x : buffers[buf]) {
      bool oe = !(x & 1);
      bool le = (x >> 1) & 1;
      unsigned int addr = (x >> 2) & ((1 << D::addr_bits) - 1);
      unsigned int data = (x >> (2 + D::addr_bits)) & ((1 << D::data_bits) - 1);

      shift_reg_back[shift_reg_ptr] = data;

      if (oe) {
        REQUIRE(!le);

        // addr is consistent curing oe pulse
        if (oe_clocks == 0)
          oe_addr = addr;
        else
          REQUIRE(addr == oe_addr);

        oe_clocks++;
      } else {
        if (oe_clocks) {
          // we've finished an oe pulse, with shift_reg_front loaded into the
          // column drivers (because le was not asserted during the pulse) and
          // the address lines set to oe_addr

          // where the display was lit during this oe pulse, brighten the result
          for (size_t row = 0; row < D::rows; row++)
            for (size_t col = 0; col < D::cols; col++)
              for (size_t color = 0; color < D::colors; color++) {
                DataAddr dataaddr = D::encode(row, col, color);
                if (dataaddr.addr == oe_addr &&
                    ((shift_reg_front[dataaddr.word] >> dataaddr.bit) & 1))
                  res((int)row, (int)col, (int)color) += oe_clocks;
              }

          oe_clocks = 0;
        }
      }

      if (le)
        for (size_t i = 0; i < D::data_words; i++)
          shift_reg_front[i] =
              shift_reg_back[(shift_reg_ptr + D::data_words - i) %
                             D::data_words];

      shift_reg_ptr = (shift_reg_ptr + 1) % D::data_words;
    }

    return res;
  }
};

/// check that if we write image, then that image is displayed correctly.
/// multiplier is the scaling factor from the written value to the number of
/// clocks that oe is held low for while that pixel is lit
template <typename D, typename Driver>
void run_test(Driver &driver, Image &image, int multiplier) {
  for (size_t row = 0; row < D::rows; row++)
    for (size_t col = 0; col < D::cols; col++)
      for (size_t color = 0; color < D::colors; color++) {
        unsigned int r = image((int)row, (int)col, 0);
        unsigned int g = image((int)row, (int)col, 1);
        unsigned int b = image((int)row, (int)col, 2);
        if (r || g || b) driver.write_rgb(row, col, r, g, b);
      }

  auto res = driver.pin_driver.template decode<D>(0);

  for (size_t row = 0; row < D::rows; row++)
    for (size_t col = 0; col < D::cols; col++)
      for (size_t color = 0; color < D::colors; color++) {
        unsigned int expected_brightness =
            multiplier * image((int)row, (int)col, (int)color);
        unsigned int actual_brightness = res((int)row, (int)col, (int)color);
        REQUIRE(actual_brightness == expected_brightness);
      }
}

TEST_CASE("single_pixels") {
  for (int row = 0; row < 2; row++)
    for (int col = 0; col < 2; col++)
      for (int color = 0; color < 3; color++)
        for (int shift = 0; shift < 8; shift++) {
          using D = FullDisplay<32, 64, 4, RGBOrder::RRGGBB>;
          Pins<D> pins{1, 2, 3, {4, 5, 6, 7}, {8, 9, 10, 11, 12, 13}};
          DisplayDriver<D, DummyDriver, false> driver(pins, 2, 8);

          Image im((int)D::rows, (int)D::cols, (int)D::colors);
          im.setZero();
          im(row, col, color) = 1 << shift;

          run_test<D>(driver, im, 2);
        }
}
