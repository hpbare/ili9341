# ILI9341
A platform-independent C++ 4-wire SPI driver for the ILI9341 LCD panel. All SPI/GPIO/delay operations are abstracted behind two interfaces (`Hal`, `Io`), so porting to a new MCU only means implementing `Hal`.

## Architecture

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

| Layer           | Role                                                                   |
|-----------------|------------------------------------------------------------|
| `ILI9341::Hal`  | Abstract interface: GPIO, delay, SPI transfer (blocking + async). Implement this base on platform. |
| `ILI9341::Io`   | Abstract transport for commands/color data. `ILI9341::SpiIo` is the ready-made SPI implementation, built on `Hal`. |
| `ILI9341::Panel`| Panel control logic (init, draw, rotate, sleep...). Only talks to `Io`/`Hal`, knows nothing about SPI or MCU specifics. |

## Usage

```c++
class MyHal : public ILI9341::Hal {
public:
    void DelayMs(uint32_t ms)                                    override { /* vTaskDelay / HAL_Delay / delay(ms) ... */ }
    void SetGpioLevel(const ILI9341::GpioSpec &gpio, bool level) override { /* gpio_set_level(...) */ }
    void ReleasePin(const ILI9341::GpioSpec &gpio)               override { /* gpio_reset_pin(...) */ }
    void ConfigureOutputPin(const ILI9341::GpioSpec &gpio)       override { /* gpio_set_direction(...) */ }
    ILI9341::Status SpiWrite(const uint8_t *data, size_t len)    override { /* blocking SPI transfer */ }
    size_t GetMaxTransferSize() override { return 4092; /* e.g. DMA descriptor limit */ }

    // Optional: override SpiWriteAsync() / SpiWaitIdle() / RegisterTransDoneCallback()
    // if your platform supports non-blocking DMA transfers.
};

MyHal hal;

ILI9341::SpiIoConfig ioConfig;
ioConfig.dcGpio = { nullptr, 4 }; // Data/Command pin (no port on ESP32-style platforms)
ILI9341::SpiIo io(hal, ioConfig);

ILI9341::Config config;
config.resetGpio     = { nullptr, 5 };
config.rgbOrder      = ILI9341::RgbElementOrder::Bgr;
config.bitsPerPixel  = ILI9341::BitsPerPixel::Bpp16;

ILI9341::Panel panel(io, hal, config);
panel.Reset();
panel.Init();
panel.DispOnOff(true);

uint16_t frameBuffer[240 * 320]; // RGB565
panel.DrawBitmap(0, 0, 240, 320, frameBuffer);

panel.SwapXY(true);
panel.Mirror(true, false); // combine with SwapXY for 90/180/270° rotation
```

## API reference

### `ILI9341::Panel`

| Method                                             | Description                                                              |
|----------------------------------------------------|--------------------------------------------------------------------------|
| `Reset()`                                          | Hardware or software reset of the panel.                                 |
| `Init()`                                           | Run the init command sequence (default or custom via `Config::initCmds`).|
| `DrawBitmap(xStart, yStart, xEnd, yEnd, colorData)`| Blit pixel data into a window region. `xEnd`/`yEnd` are exclusive.       |
| `Mirror(mirrorX, mirrorY)`                         | Mirror the display along X/Y.                                            |
| `SwapXY(swapAxes)`                                 | Swap X/Y axes. Combine with `Mirror` for rotation.                       |
| `SetGap(xGap, yGap)`                               | Offset added to coordinates before `CASET`/`RASET` (e.g. for panels with visible-area offset). |
| `InvertColor(invert)`                              | Invert display colors.                                                   |
| `DispOnOff(on)`                                    | Turn the display output on/off.                                          |
| `DispSleep(sleep)`                                 | Enter/exit sleep mode.                                                   |

### `ILI9341::Hal` (Implement this)

| Method | Description | Required? |
|-----------------------------------------|-------------------------------------------------------|-----|
| `DelayMs(ms)`                           | Block for at least `ms` milliseconds.                 | Yes |
| `SetGpioLevel(const GpioSpec&, level)`  | Drive a pin high/low.                                 | Yes |
| `ConfigureOutputPin(const GpioSpec&)`   | Configure a pin as push-pull output.                  | Yes |
| `ReleasePin(const GpioSpec&)`           | Return a pin to its unused state.                     | Yes |
| `SpiWrite(data, len)`                   | Blocking SPI write.                                   | Yes |
| `GetMaxTransferSize()`                  | Max bytes per single SPI transaction (chunking limit).| Yes |
| `SpiWriteAsync(data, len, isLastChunk)` | Non-blocking, queue-and-return write. `isLastChunk` marks the transaction whose completion should notify the caller. | No - defaults to `SpiWrite()` |
| `SpiWaitIdle()`                         | Block until all queued `SpiWriteAsync()` transfers complete. Called by `SpiIo` before changing the DC line.          | No - defaults to no-op |
| `RegisterTransDoneCallback(cb, userCtx)`| Register a callback fired (typically from ISR/DMA-done) when the last-queued color chunk finishes on the bus.        | No - defaults to no-op |

### `ILI9341::Io` / `ILI9341::SpiIo`

| Method                          | Description |
|---------------------------------|---------------------------------------------------------------------------------|
| `TxParam(cmd, param, paramSize)`| Send a command + optional parameter bytes (blocking).                           |
| `TxColor(cmd, color, colorSize)`| Send a command + bulk color data, chunked and pipelined via `SpiWriteAsync`. Returns once all chunks are *queued*, not necessarily transmitted - use `RegisterTransDoneCallback()` or `SpiWaitIdle()` to know when transfer is truly done. |
| `RxParam(cmd, param, paramSize)`| Not implemented - driver is currently write-only (`Status::ErrorNotSupported`). |

## Notes

- `TxColor()` no longer blocks until the transfer finishes - see `Hal::RegisterTransDoneCallback()` above if you need a completion signal.
- `RxParam` is unimplemented; the driver is write-only for now.
- GPIO pins are identified by `ILI9341::GpioSpec { void *port; int pin; }` rather than a plain `int`, so both flat-numbering platforms (ESP32: leave `port = nullptr`) and port+pin platforms (STM32: e.g. `{ GPIOB, GPIO_PIN_5 }`) can represent a pin natively without bit-packing.

## License

See the [LICENSE](./LICENSE) file.