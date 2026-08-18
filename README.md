# ILI9341
A C++ 4-wire SPI library for the ILI9341 LCD panel, designed to be platform-independent. All SPI communication and GPIO/delay operations are abstracted behind interfaces (`Hal`, `Io`), so users only need to implement these interfaces base on their own platform.
## Project structure
```text
📁 ili9341/
├── ili9341.cpp             # Panel class implementation (LCD control logic)
├── ili9341.hpp             # Panel class declaration (public API)
├── ili9341_cmds.hpp        # ILI9341 command table and MADCTL bits
├── ili9341_io.cpp          # SpiIo transport implementation
├── ili9341_io.hpp          # SpiIo transport declaration
├── ili9341_types.hpp       # Data types, enums, Hal/Io interfaces, Config
├── README.md               # Project descriptions
└── LICENSE
```

## Architecture
The driver is split into three clearly separated responsibilities:
- **`ILI9341::Hal`** (abstract interface) - abstracts the hardware: GPIO, delay, and SPI transfer (blocking + non-blocking). This is the part **users must implement** base on their platform.
- **`ILI9341::Io`** (abstract interface) - abstracts the transport used to send commands/data to the panel. `ILI9341::SpiIo` is a ready-made implementation for SPI communication (built on top of `Hal`).
- **`ILI9341::Panel`** - the main class, containing all panel control logic. `Panel` only interacts through `Io` and `Hal`, and knows nothing about SPI or any specific MCU.

```
       ┌────────────────┐
       │ ILI9341::Panel │  ← LCD control logic (public API)
       └──────┬─────────┘
              │ uses
       ┌──────▼─────────┐
       │  ILI9341::Io   │  ← command/color transport (interface)
       └──────┬─────────┘
              │ default implementation
       ┌──────▼─────────┐
       │ ILI9341::SpiIo │
       └──────┬─────────┘
              │ uses
       ┌──────▼─────────┐
       │  ILI9341::Hal  │  ← GPIO/delay/SPI transfer (user-implemented)
       └────────────────┘
```

## Features

- Panel initialization with a default command sequence, or a custom `initCmds` passed through `Config`.
- Draw a bitmap into a window region (`DrawBitmap`), supporting both RGB565 (16bpp) and RGB666 (18bpp) pixel formats.
- Mirror the display along the X/Y axis (`Mirror`) and swap X/Y axes (`SwapXY`) - combine both to achieve 90/180/270-degree rotation.
- Invert colors (`InvertColor`), turn the display on/off (`DispOnOff`), and enter/exit sleep mode (`DispSleep`).
- Compensate the drawing offset with `SetGap` (useful for panels whose visible area is offset from RAM, e.g. some 240x240 modules).
- Chunked color transfer via `SpiWriteAsync`/`SpiWaitIdle`, optimized for DMA and hardware transfer-size limits.
- Debug mode (`ILI9341_DEBUG`) warns when a MADCTL/COLMOD command in a custom `initCmds` is overwritten by the driver's own command.

## Installation

Copy the entire `ili9341/` directory into your project, add it to your include path, and implement the `ILI9341::Hal` interface for your platform.

## Usage

### 1. Implement `Hal` for your platform

```c++
class MyHal : public ILI9341::Hal {
public:
    void DelayMs(uint32_t ms) override { /* vDelay / HAL_Delay / delay(ms) ... */ }
    void SetGpioLevel(int gpio, bool level) override { /* gpio_set_level(...) */ }
    void ReleasePin(int gpio) override { /* gpio_reset_pin(...) */ }
    void ConfigureOutputPin(int gpio) override { /* gpio_set_direction(...) */ }
    ILI9341::Status SpiWrite(const uint8_t *data, size_t len) override { /* blocking SPI transfer */ }
    size_t GetMaxTransferSize() override { return 4092; /* e.g. DMA descriptor limit */ }

    // Optional: override SpiWriteAsync()/SpiWaitIdle() if your platform supports non-blocking DMA
};
```

### 2. Set up `SpiIo` and `Panel`

```c++
MyHal hal;

ILI9341::SpiIoConfig ioConfig;
ioConfig.dcGpio = 4; // Data/Command pin

ILI9341::SpiIo io(hal, ioConfig);

ILI9341::Config config;
config.resetGpioNum   = 5;
config.rgbOrder        = ILI9341::RgbElementOrder::Bgr;
config.bitsPerPixel    = ILI9341::BitsPerPixel::Bpp16;
// config.initCmds / config.initCmdsSize if you want a custom init sequence

ILI9341::Panel panel(io, hal, config);

panel.Reset();
panel.Init();
panel.DispOnOff(true);
```

### 3. Draw to the screen

```c++
uint16_t frameBuffer[240 * 320]; // RGB565, allocate/fill this yourself

panel.DrawBitmap(0, 0, 240, 320, frameBuffer);
```

### 4. Rotate / mirror the screen

```c++
panel.SwapXY(true);
panel.Mirror(true, false);
```

## Notes
- `Panel::DrawBitmap` automatically adds the gap set via `SetGap` to the coordinates before sending the `CASET`/`RASET` commands.
- `SpiIo::RxParam` is currently not supported (`Status::ErrorNotSupported`) - the driver is currently write-only.

## License

See the [LICENSE](./LICENSE) file.