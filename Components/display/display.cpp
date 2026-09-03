#include "display.h"
#include "ili9341.hpp"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "ili9341_io.hpp"
#include "ili9341_types.hpp"
#include "main.h"
#include "projdefs.h"
#include "spi.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include "task.h"
#include <cstddef>
#include <cstdint>

/* === ILI9341 === */

class DisplayHal : public ILI9341::Hal {
public:
    void DelayMs(uint32_t ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void SetGpioLevel(const ILI9341::GpioSpec &gpio, bool level) override {
        HAL_GPIO_WritePin(
            static_cast<GPIO_TypeDef*>(gpio.port),
            gpio.pin,
            level ? GPIO_PIN_SET : GPIO_PIN_RESET
        );
    }

    void ReleasePin(const ILI9341::GpioSpec &gpio) override {
        HAL_GPIO_DeInit(static_cast<GPIO_TypeDef*>(gpio.port), gpio.pin);
    }

    void ConfigureOutputPin(const ILI9341::GpioSpec &gpio) override {
        GPIO_InitTypeDef g{};
        g.Pin   = gpio.pin;
        g.Mode  = GPIO_MODE_OUTPUT_PP;
        g.Pull  = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(static_cast<GPIO_TypeDef*>(gpio.port), &g);
    }

    ILI9341::Status SpiWrite(const uint8_t *data, size_t len) override {
        HAL_StatusTypeDef s = HAL_SPI_Transmit(&hspi1, (uint8_t*)data, (uint16_t)len, HAL_MAX_DELAY);
        return (s == HAL_OK) ?  ILI9341::Status::Ok : ILI9341::Status::ErrorIo;
    }

    size_t GetMaxTransferSize() override {
        return 4096;
    }
};

namespace {
    DisplayHal              g_hal;
    ILI9341::SpiIoConfig    g_ioConfig;
    ILI9341::SpiIo          *g_io = nullptr;
    ILI9341::Config         g_config;
    ILI9341::Panel          *g_panel = nullptr;
}

static int display_init_ili9341(void){
    g_ioConfig.dcGpio = {ILI9341_EXAMPLE_DC_GPIO_Port, ILI9341_EXAMPLE_DC_Pin};
    static ILI9341::SpiIo io(g_hal, g_ioConfig);
    g_io = &io;

    g_config.resetGpio = {ILI9341_EXAMPLE_RST_GPIO_Port, ILI9341_EXAMPLE_RST_Pin};
    g_config.rgbOrder = ILI9341::RgbElementOrder::Rgb;
    g_config.bitsPerPixel = ILI9341::BitsPerPixel::Bpp16;

    static ILI9341::Panel panel(*g_io, g_hal, g_config);
    g_panel = &panel;

    ILI9341::Status s;

    s = panel.Reset();
    if(s != ILI9341::Status::Ok){
        return static_cast<int>(s);
    }

    s = panel.Init();
    if(s != ILI9341::Status::Ok){
        return static_cast<int>(s);
    }

    panel.DispOnOff(true);
    if(s != ILI9341::Status::Ok){
        return static_cast<int>(s);
    }

    g_panel = &panel;
    return 0;
}

/* === LVGL === */















int display_init(void)
{
    return display_init_ili9341();
}
