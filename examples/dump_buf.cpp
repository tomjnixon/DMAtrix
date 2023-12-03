#include <dmatrix/buffer_model.h>
#include <dmatrix/display_model.h>

#include <cassert>
#include <iostream>

using namespace DMAtrix;

// print some information about a buffer on stderr, then dump it to stdout
//
// view length/brightness etc.:
//    dump_buf > /dev/null
//
// view generated waveforms:
//    dump_buf > /tmp/out.csv
//    pulseview -Icsv:samplerate=20000000:header=true /tmp/out.csv

using D = FullDisplay<32, 64, 4>;
using B = BufferModel<D>;
B b(4, 8);

int main(int argc, char **argv) {
  using buf_t = uint16_t;
  buf_t *buf = new buf_t[b.buf_len];
  b.init_buffer(buf);

  for (size_t row = 0; row < D::rows; row++)
    for (size_t col = 0; col < D::cols; col++)
      b.write_rgb<uint16_t, 16>(buf, row, col, 0xffff, 0xffff, 0xffff);

  std::cerr << "length: " << b.buf_len << std::endl;
  std::cerr << "bytes: " << sizeof(buf_t) * b.buf_len << std::endl;
  std::cerr << "freq at 20MHz: " << 20.0e6 / (double)b.buf_len << std::endl;

  size_t cycles_on = 0;
  for (size_t i = 0; i < b.buf_len; i++) {
    if (!(buf[i] & (1 << b.oe_bit()))) cycles_on++;
  }
  std::cerr << "brightness: " << ((double)cycles_on) / ((double)b.buf_len)
            << std::endl;

  std::cout << "clk,oe,le";
  for (size_t i = 0; i < D::addr_bits; i++) std::cout << ",addr[" << i << "]";
  for (size_t i = 0; i < D::data_bits; i++) std::cout << ",data[" << i << "]";
  std::cout << std::endl;

  for (size_t i = 0; i < b.buf_len; i++) {
    for (size_t clk = 0; clk < 2; clk++) {
      std::cout << clk << "," << ((buf[i] & (1 << b.oe_bit())) ? 0 : 1) << ","
                << ((buf[i] & (1 << b.le_bit())) ? 1 : 0);
      for (size_t j = 0; j < D::addr_bits; j++)
        std::cout << "," << ((buf[i] & (1 << b.addr_bit(j))) ? 1 : 0);
      for (size_t j = 0; j < D::data_bits; j++)
        std::cout << "," << ((buf[i] & (1 << b.data_bit(j))) ? 1 : 0);

      std::cout << std::endl;
    }
  }
}
