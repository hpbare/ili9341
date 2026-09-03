#include "stm32_hal.hpp"
#include <cstdint>

/* =========================================================================
 * GPIO pin encoding
 *
 * STM32 GPIO ports (GPIOA … GPIOK) are mapped to indices 0…10.
 * Encoded value = (portIndex << 16) | pin16bit
 * ========================================================================= */

static constexpr GPIO_TypeDef *kGpioPorts[] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE,
#ifdef GPIOF
    GPIOF,
#else
    nullptr,
#endif
#ifdef GPIOG
    GPIOG,
#else
    nullptr,
#endif
#ifdef GPIOH
    GPIOH,
#else
    nullptr,
#endif
};
static constexpr int kGpioPortCount = static_cast<int>(sizeof(kGpioPorts) / sizeof(kGpioPorts[0]));

/* static */ int Stm32Hal::EncodePin(GPIO_TypeDef *port, uint16_t pin)
{
    for (int i = 0; i < kGpioPortCount; ++i)
    {
        if (kGpioPorts[i] == port)
        {
            return (i << 16) | static_cast<int>(pin);
        }
    }
    return -1; /* Unknown port */
}

/* static */ void Stm32Hal::DecodePin(int encoded, GPIO_TypeDef **portOut, uint16_t *pinOut)
{
    int portIdx = (encoded >> 16) & 0xFF;
    *pinOut     = static_cast<uint16_t>(encoded & 0xFFFF);
    if (portIdx >= 0 && portIdx < kGpioPortCount)
    {
        *portOut = kGpioPorts[portIdx];
    }
    else
    {
        *portOut = nullptr;
    }
}

/* =========================================================================
 * Constructor
 * ========================================================================= */

Stm32Hal::Stm32Hal(SPI_HandleTypeDef *hspi, uint32_t spiTimeoutMs)
    : hspi_(hspi), spiTimeoutMs_(spiTimeoutMs)
{
}

/* =========================================================================
 * ILI9341::Hal interface
 * ========================================================================= */

void Stm32Hal::DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void Stm32Hal::SetGpioLevel(int gpio, bool level)
{
    GPIO_TypeDef *port;
    uint16_t      pin;
    DecodePin(gpio, &port, &pin);
    if (port)
    {
        HAL_GPIO_WritePin(port, pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void Stm32Hal::ReleasePin(int /*gpio*/)
{
    /* Nothing to do for STM32 – pin direction controlled by CubeMX at startup */
}

void Stm32Hal::ConfigureOutputPin(int gpio)
{
    GPIO_TypeDef *port;
    uint16_t      pin;
    DecodePin(gpio, &port, &pin);
    if (!port) return;

    GPIO_InitTypeDef cfg = {};
    cfg.Pin   = pin;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &cfg);
}

ILI9341::Status Stm32Hal::SpiWrite(const uint8_t *data, size_t len)
{
    /* HAL_SPI_Transmit takes uint16_t Size */
    while (len > 0)
    {
        uint16_t chunk = (len > 0xFFFF) ? 0xFFFF : static_cast<uint16_t>(len);
        HAL_StatusTypeDef ret = HAL_SPI_Transmit(
            hspi_,
            const_cast<uint8_t *>(data),
            chunk,
            spiTimeoutMs_);
        if (ret != HAL_OK)
        {
            return ILI9341::Status::ErrorIo;
        }
        data += chunk;
        len  -= chunk;
    }
    return ILI9341::Status::Ok;
}

ILI9341::Status Stm32Hal::SpiWriteAsync(const uint8_t *data, size_t len, bool isLastChunk)
{
    /* Use DMA only if the SPI handle has a Tx DMA stream configured */
    if (hspi_->hdmatx == nullptr)
    {
        return SpiWrite(data, len);
    }

    /* Wait for any previous async transfer to finish before queuing the next */
    ILI9341::Status s = SpiWaitIdle();
    if (s != ILI9341::Status::Ok) return s;

    /* DMA transfer is limited to 65535 bytes per call */
    uint16_t chunk = (len > 0xFFFF) ? 0xFFFF : static_cast<uint16_t>(len);

    txDone_      = false;
    isLastChunk_ = isLastChunk;

    HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(
        hspi_,
        const_cast<uint8_t *>(data),
        chunk);

    if (ret != HAL_OK)
    {
        txDone_ = true;
        return ILI9341::Status::ErrorIo;
    }

    /* If chunk was smaller than len (> 64 kB), fall back to blocking for remainder */
    if (chunk < len)
    {
        SpiWaitIdle();
        return SpiWrite(data + chunk, len - chunk);
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status Stm32Hal::SpiWaitIdle()
{
    /* Spin-wait; in a FreeRTOS context you could yield with taskYIELD() */
    while (!txDone_)
    {
        /* Optionally: osDelay(1); */
    }
    return ILI9341::Status::Ok;
}

void Stm32Hal::RegisterTransDoneCallback(ILI9341::Hal::TransDoneCallback cb, void *userCtx)
{
    transDoneCb_  = cb;
    transDoneCtx_ = userCtx;
}

size_t Stm32Hal::GetMaxTransferSize()
{
    /* STM32F4 DMA: 16-bit NDTR register → max 65535 bytes per transfer */
    return 65535u;
}

void Stm32Hal::OnTxComplete()
{
    txDone_ = true;
    if (isLastChunk_ && transDoneCb_)
    {
        transDoneCb_(transDoneCtx_);
    }
}
