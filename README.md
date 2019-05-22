# DMAtrix

Yet another library for driving dumb LED matrices. This one is designed to
support multiple display types and platforms without complicated code.

Currently only simple displays are supported on ESP32. This is an early-stage
project; expect the API to change as more features are added.

## Documentation

There isn't much, yet. [examples/hub75_esp32.cpp](examples/hub75_esp32.cpp)
demonstrates the full API.

## Technical Overview

This section describes the structure of the library.

### Display Model

The display model represents the type of display that is being used. It has a
static method `encode`, which maps from a display coordinate (row, column,
color channel) to a position within the data stream ('row select' address, bit,
words before LE), represented by a `DataAddr` struct.

Adding a new display type should just require implementation of a corresponding
display model class.

Templates and inheritance make this easier, for example, defining a 32x64
display with 4 address lines (and therefore 6 data lines):

```cpp
using Display = FullDisplay<32, 64, 4, RGBOrder::RRGGBB>;
```

If you'd instead wired this up by wiring R1 out to R2 in, R2 out to G1 in etc.
this can be wrapped in a `WrappedDisplay` class, which results in a display model
with only one data line:

```cpp
using Display = WrappedDisplay<FullDisplay<32, 64, 4, RGBOrder::RRGGBB>>;
```

(note that this isn't a great idea with this library, as it will use more
memory and refresh slower, but might be handy if you've already wired up your
display using the instructions from P10_matrix, like I had)

The builtin display models are currently quite limited, but can easily be
expanded to support more panel geometries, rotation, multiple panels etc.

### Buffer Model

The buffer model represents the waveform used to drive the display. It decides
how the sub-frames should be packed together, resulting in an offset for each
subframe and the total amount of DMA memory required. It has methods to
initialise a provided DMA buffer with the static parts of the waveforms
(address lines, LE, OE) and to write pixel data into a provided DMA buffer.

Writing pixel data is reasonably fast; about 200fps on ESP32 with a 32x64
display.

The buffer model has two parameters:

-   The bit depth for each color channel. Arbitrary bit depths are supported;
    this is helpful because 8 bits isn't enough to make good looking gradients,
    but 16 would reduce the refresh rate too much. For example, with a 32x64
    panel at 20MHz, 12 bits can be refreshed at 282Hz -- probably acceptable,
    while 16 would drop down to 19Hz, which is too slow.

-   The length in clocks of the LSB; all other bits are this long multiplied by
    a power of two. This allows trading of refresh rate for brightness:

    -   When the LSB is being displayed, loading data takes more time than the
        OE pulse, so the display is not as bright as it could be if longer
        pulses were used.

    -   When the MSB is being displayed, the OE pulse takes more time than
        loading the data, so longer pulses result in a slower overall refresh
        rate.

    For example, with a 32x64 display and 8 bits, an LSB of one clock results
    in a refresh rate of 2163Hz at 44% brightness, but 4 clocks results in a
    refresh rate of 1024Hz and a brightness of 84% -- approximately half the
    refresh rate for twice the brightness.

### DMA Driver

A DMA driver is required for each supported platform. It allocates blocks of
DMA memory, and squirts them out on the given pins at the configured rate. DMA
drivers can allocate multiple blocks of memory and switch cleanly between them
to implement double buffering.

The interface to DMA drivers is designed to deal with the quirks of the ESP32
I2S hardware, but this is quite quirky so it should be possible to implement
the same interface on other platforms. The main affordance is that the type
representing the buffer is decided by the DMA class (given the number of bits
required). This allows a class to be used instead of a raw pointer, which can
implement reordering of bits required by odd DMA hardware.

#### ESP32 DMA

The ESP32 DMA driver is based on the `esp32_i2s_parallel` code from Sprite_tm,
with fixes to make it work nicely in 8 bit mode and with I2S0. It supports 8
and 16 bit DMA (32 bit should be possible too) on arbitrary pins at 20MHz.

### Display Driver

The display driver class ties together the other components and provides the
external API. It instantiates the buffer model and the DMA driver, initialises
the buffers, and wraps up calls to the buffer model with the correct buffer.

The template parameters are:

- The display model type.

- The DMA driver type.

- A flag to enable double buffering. Double buffering is not always required --
  if refreshes and drawing are both fast enough, tearing will not be visible
  even without it. Enabling double buffering uses twice the DMA memory.

## Development

Tests can be built and ran locally using meson:

```shell
meson builddir
meson test -C builddir
```

The `dump_buf` program may be useful for viewing the output waveforms in
pulseview, without having to capture them from hardware; modify
[examples/dump_buf.cpp](examples/dump_buf.cpp), then:

```shell
ninja -C builddir dump_buf
builddir/dump_buf > /tmp/out.csv
pulseview -Icsv:samplerate=20000000:header=true /tmp/out.csv
```

## Other Projects

This of course isn't the only library for these displays; reading the code of
other projects was a massive help in getting this working. Some of them are
listed here:

- https://github.com/phkehl/esp32-leddisplay
- https://github.com/2dom/P10_matrix
- https://github.com/VGottselig/ESP32-RGB-Matrix-Display
- https://github.com/pixelmatix/SmartMatrix/tree/teensylc
