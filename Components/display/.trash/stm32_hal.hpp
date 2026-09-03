#ifndef STM32_HAL_HPP_
#define STM32_HAL_HPP_

#include "ili9341_types.hpp"
#include "stm32f4xx_hal.h"

/**
 * @file    stm32_hal.hpp
 * @brief   Concrete ILI9341::Hal implementation for STM32F4 using STM32 HAL.
 *
 * Wraps HAL_SPI_Transmit (blocking) and, optionally, HAL_SPI_Transmit_DMA
 * for async colour transfers.  GPIO is driven via HAL_GPIO_WritePin.
 *
 * Usage
 * -----
 * @code
 *   Stm32Hal hal(&hspi1, SPI_TIMEOUT_MS);
 *   // Optional: register DMA-done callback for async flush
 *   hal.RegisterTransDoneCallback(MyCallback, myCtx);
 * @endcode
 */
class Stm32Hal : public ILI9341::Hal
{
public:
    /**
     * @param hspi          Pointer to the initialised SPI handle (e.g. &hspi1).
     * @param spiTimeoutMs  Timeout in ms for blocking HAL_SPI_Transmit calls.
     *                      Ignored when DMA is used.
     */
    explicit Stm32Hal(SPI_HandleTypeDef *hspi, uint32_t spiTimeoutMs = 100);

    /* ------------------------------------------------------------------ */
    /* ILI9341::Hal interface                                               */
    /* ------------------------------------------------------------------ */

    void            DelayMs(uint32_t ms) override;
    void            SetGpioLevel(int gpio, bool level) override;
    void            ReleasePin(int gpio) override;
    void            ConfigureOutputPin(int gpio) override;
    ILI9341::Status SpiWrite(const uint8_t *data, size_t len) override;

    /**
     * @brief Non-blocking SPI write via DMA (if SPI handle has DMA configured).
     *        Falls back to blocking SpiWrite() when DMA is not configured.
     * @note  The TransDoneCallback registered via RegisterTransDoneCallback()
     *        must be wired to HAL_SPI_TxCpltCallback() by the application.
     */
    ILI9341::Status SpiWriteAsync(const uint8_t *data, size_t len, bool isLastChunk) override;

    /** @brief Block until the DMA transfer completes (polls txDone_ flag). */
    ILI9341::Status SpiWaitIdle() override;

    /**
     * @brief Register a callback invoked from HAL_SPI_TxCpltCallback()
     *        when the last DMA chunk of a TxColor() transfer finishes.
     */
    void RegisterTransDoneCallback(ILI9341::Hal::TransDoneCallback cb, void *userCtx) override;

    /**
     * @return Maximum bytes per DMA descriptor (64 KB – 1 for STM32F4 DMA).
     *         Limits the chunk size in SpiIo::TxColor.
     */
    size_t GetMaxTransferSize() override;

    /* ------------------------------------------------------------------ */
    /* Called from HAL_SPI_TxCpltCallback() in stm32f4xx_it.c / freertos.c */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Must be called from HAL_SPI_TxCpltCallback() to signal transfer done.
     *        If this is the last chunk (isLastChunk_ == true), fires the
     *        registered TransDoneCallback.
     */
    void OnTxComplete();

    /* ------------------------------------------------------------------ */
    /* GPIO pin encoding                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Pack a GPIO port + pin pair into a single int used as the
     *        `gpio` argument throughout ILI9341::Hal.
     *
     * Example:
     * @code
     *   int dcPin = Stm32Hal::EncodePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
     * @endcode
     */
    static int EncodePin(GPIO_TypeDef *port, uint16_t pin);

    /** @brief Decode a pin encoded by EncodePin() back to port + pin. */
    static void DecodePin(int encoded, GPIO_TypeDef **portOut, uint16_t *pinOut);

private:
    SPI_HandleTypeDef *hspi_;
    uint32_t           spiTimeoutMs_;

    /* DMA / async state */
    volatile bool      txDone_       = true;
    volatile bool      isLastChunk_  = false;
    TransDoneCallback  transDoneCb_  = nullptr;
    void              *transDoneCtx_ = nullptr;
};

#endif /* STM32_HAL_HPP_ */
