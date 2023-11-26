#pragma once

#include <driver/gpio.h>
#include <driver/periph_ctrl.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <rom/gpio.h>
#include <rom/lldesc.h>
#include <soc/gpio_periph.h>
#include <soc/gpio_sig_map.h>
#include <soc/i2s_reg.h>
#include <soc/i2s_struct.h>
#include <array>

namespace DMAtrix {

  namespace esp32 {

    const size_t DMA_MAX = 4096 - 4;

    inline void setup_descriptors(volatile lldesc_t *dmadesc, size_t desccount,
                                  uint8_t *buf, size_t buf_len) {
      for (size_t i = 0; i < desccount; i++) {
        size_t offset = i * DMA_MAX;
        size_t size = std::min(buf_len - offset, DMA_MAX);

        dmadesc[i].size = DMA_MAX;
        dmadesc[i].length = size;
        dmadesc[i].buf = buf + offset;
        dmadesc[i].eof = 0;
        dmadesc[i].sosf = 0;
        dmadesc[i].owner = 1;
        dmadesc[i].qe.stqe_next = (lldesc_t *)dmadesc + (i + 1) % desccount;
        dmadesc[i].offset = 0;
      }

      dmadesc[desccount - 1].eof = 1;
    }

    inline void gpio_setup_out(int gpio, int sig) {
      if (gpio == -1) return;
      PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
      gpio_set_direction((gpio_num_t)gpio, GPIO_MODE_OUTPUT);
      gpio_matrix_out((gpio_num_t)gpio, sig, false, false);
    }

    inline void dma_reset(i2s_dev_t *dev) {
      dev->lc_conf.in_rst = 1;
      dev->lc_conf.in_rst = 0;
      dev->lc_conf.out_rst = 1;
      dev->lc_conf.out_rst = 0;
    }

    inline void fifo_reset(i2s_dev_t *dev) {
      dev->conf.rx_fifo_reset = 1;
      dev->conf.rx_fifo_reset = 0;
      dev->conf.tx_fifo_reset = 1;
      dev->conf.tx_fifo_reset = 0;
    }

    template <typename T>
    struct DMABuffer {
      T *buf = nullptr;
      size_t desccount;
      lldesc_t *dmadesc;

      T &operator[](size_t idx) {
        if (std::is_same<T, uint8_t>::value)
          return buf[idx ^ 2];
        else if (std::is_same<T, uint16_t>::value)
          return buf[idx ^ 1];
        else
          return buf[idx];
      }

      void setup(size_t size) {
        buf = (T *)heap_caps_malloc(sizeof(T) * size, MALLOC_CAP_DMA);
        assert(buf);

        desccount = (size * sizeof(T) + (DMA_MAX - 1)) / DMA_MAX;
        dmadesc = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t),
                                               MALLOC_CAP_DMA);
        assert(dmadesc);

        setup_descriptors(dmadesc, desccount, (uint8_t *)buf, sizeof(T) * size);
      }
    };

    i2s_dev_t *i2s_dev(size_t num) {
      assert(num == 0 || num == 1);

      return num ? &I2S1 : &I2S0;
    }

    struct ISRInfo {
      size_t dev;
      volatile bool flip_done;
    };

    void IRAM_ATTR i2s_isr_ext(void *arg) {
      ISRInfo *isr_info = (ISRInfo *)arg;

      i2s_dev_t *dev = isr_info->dev ? &I2S1 : &I2S0;

      dev->int_clr.out_eof = 1;

      isr_info->flip_done = true;
    }

  }

  struct ESP32Config {
    size_t dev = 0;
    int clkspeed_hz = 20000000;
  };

  template <size_t num_pins, size_t num_buffers>
  struct ESP32I2SDMA {
    static_assert(num_pins <= 16, "DMA in 32 bit mode is not yet supported");
    using dtype = typename std::conditional_t<
        num_pins <= 8, uint8_t,
        typename std::conditional_t<num_pins <= 16, uint16_t, uint32_t>>;
    std::array<esp32::DMABuffer<dtype>, num_buffers> buffers;

    using Config = ESP32Config;

    esp32::ISRInfo isr_info;

    void flip_to(size_t buf_idx) {
      for (auto &buf : buffers)
        buf.dmadesc[buf.desccount - 1].qe.stqe_next = buffers[buf_idx].dmadesc;
      isr_info.flip_done = false;
    }

    bool flip_done() { return isr_info.flip_done; }

    void setup(std::array<int, num_pins> data_pins, int clk_pin, Config config,
               size_t size) {
      for (auto &buffer : buffers) buffer.setup(size);

      i2s_dev_t *dev = esp32::i2s_dev(config.dev);
      constexpr size_t bits = sizeof(dtype) * 8;

      // Figure out which signal numbers to use for routing
      int sig_data_base, sig_clk;
      if (dev == &I2S0) {
        sig_data_base = I2S0O_DATA_OUT8_IDX;
        sig_clk = I2S0O_WS_OUT_IDX;
      } else {
        sig_data_base = I2S1O_DATA_OUT8_IDX;
        sig_clk = I2S1O_WS_OUT_IDX;
      }

      // Route the signals
      for (int i = 0; i < num_pins; i++) {
        esp32::gpio_setup_out(data_pins[i], sig_data_base + i);
      }
      esp32::gpio_setup_out(clk_pin, sig_clk);

      // Power on dev
      if (dev == &I2S0) {
        periph_module_enable(PERIPH_I2S0_MODULE);
      } else {
        periph_module_enable(PERIPH_I2S1_MODULE);
      }
      // Initialize I2S dev
      dev->conf.rx_reset = 1;
      dev->conf.rx_reset = 0;
      dev->conf.tx_reset = 1;
      dev->conf.tx_reset = 0;
      esp32::dma_reset(dev);
      esp32::fifo_reset(dev);

      // Enable LCD mode
      dev->conf2.val = 0;
      dev->conf2.lcd_en = 1;

      // Enable "One datum will be written twice in LCD mode" - for some reason,
      // if we don't do this in 8-bit mode, data is updated on half-clocks not
      // clocks
      if (bits == 8) dev->conf2.lcd_tx_wrx2_en = 1;

      dev->sample_rate_conf.val = 0;
      dev->sample_rate_conf.rx_bits_mod = bits;
      dev->sample_rate_conf.tx_bits_mod = bits;
      // ToDo: Unsure about what this does...
      dev->sample_rate_conf.rx_bck_div_num = 4;

      // because conf2.lcd_tx_wrx2_en is set for 8-bit mode, the clock speed is
      // doubled, drop it in half here
      if (bits == 8)
        dev->sample_rate_conf.tx_bck_div_num = 2;
      else
        // datasheet says this must be 2 or greater (but 1 seems to work)
        dev->sample_rate_conf.tx_bck_div_num = 1;

      dev->clkm_conf.val = 0;
      dev->clkm_conf.clka_en = 0;
      dev->clkm_conf.clkm_div_a = 63;
      dev->clkm_conf.clkm_div_b = 63;
      // We ignore the possibility for fractional division here, clkspeed_hz
      // must round up for a fractional clock speed, must result in >= 2
      dev->clkm_conf.clkm_div_num = 80000000L / (config.clkspeed_hz + 1);

      dev->fifo_conf.val = 0;
      dev->fifo_conf.rx_fifo_mod_force_en = 1;
      dev->fifo_conf.tx_fifo_mod_force_en = 1;
      // dev->fifo_conf.tx_fifo_mod=1;
      dev->fifo_conf.tx_fifo_mod = 1;
      dev->fifo_conf.rx_data_num = 32;  // Thresholds.
      dev->fifo_conf.tx_data_num = 32;
      dev->fifo_conf.dscr_en = 1;

      dev->conf1.val = 0;
      dev->conf1.tx_stop_en = 0;
      dev->conf1.tx_pcm_bypass = 1;

      dev->conf_chan.val = 0;
      dev->conf_chan.tx_chan_mod = 1;
      dev->conf_chan.rx_chan_mod = 1;

      // Invert ws to be active-low... ToDo: make this configurable
      // dev->conf.tx_right_first=1;
      dev->conf.tx_right_first = 0;
      // dev->conf.rx_right_first=1;
      dev->conf.rx_right_first = 0;

      dev->timing.val = 0;

      // Reset FIFO/DMA -> needed? Doesn't dma_reset/fifo_reset do this?
      dev->lc_conf.in_rst = 1;
      dev->lc_conf.out_rst = 1;
      dev->lc_conf.ahbm_rst = 1;
      dev->lc_conf.ahbm_fifo_rst = 1;
      dev->lc_conf.in_rst = 0;
      dev->lc_conf.out_rst = 0;
      dev->lc_conf.ahbm_rst = 0;
      dev->lc_conf.ahbm_fifo_rst = 0;
      dev->conf.tx_reset = 1;
      dev->conf.tx_fifo_reset = 1;
      dev->conf.rx_fifo_reset = 1;
      dev->conf.tx_reset = 0;
      dev->conf.tx_fifo_reset = 0;
      dev->conf.rx_fifo_reset = 0;

      isr_info.dev = config.dev;
      isr_info.flip_done = false;

      // setup I2S Interrupt
      dev->int_ena.out_eof = 1;

      // allocate a level 1 intterupt: lowest priority, as ISR isn't urgent
      int int_no = dev == &I2S1 ? ETS_I2S1_INTR_SOURCE : ETS_I2S0_INTR_SOURCE;
      esp_intr_alloc(int_no, (int)(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1),
                     esp32::i2s_isr_ext, (void *)&isr_info, NULL);

      // Start dma on front buffer
      dev->lc_conf.val =
          I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
      dev->out_link.addr = (uint32_t)buffers[0].dmadesc;
      dev->out_link.start = 1;
      dev->conf.tx_start = 1;
    }
  };

}
