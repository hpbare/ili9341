#include "ili9341_io.hpp"

ILI9341::SpiIo::SpiIo(ILI9341::Hal &hal, const ILI9341::SpiIoConfig &config)
    : hal_(hal), config_(config)
{
    if (this->config_.maxChunkBytes == 0)
    {
        this->config_.maxChunkBytes = this->hal_.GetMaxTransferSize();
    }
    // config_.flags.dcCmdLevel / dcParamLevel / dcDataLevel are used directly
    // in TxParam / TxColor – no need to copy them into a separate flags_ field.
}

ILI9341::Status ILI9341::SpiIo::TxParam(int cmd, const void *param, size_t paramSize)
{
    /* Drain any in-flight SpiWriteAsync() transfer before touching the DC pin
     * or starting a new SPI transaction – changing DC mid-transfer corrupts
     * the current pixel burst. */
    this->hal_.SpiWaitIdle();

    if (cmd >= 0)
    {
        this->hal_.SetGpioLevel(this->config_.dcGpio, this->config_.flags.dcCmdLevel);
        uint8_t cmdByte = static_cast<uint8_t>(cmd);
        ILI9341::Status s = this->hal_.SpiWrite(&cmdByte, 1);
        if (s != ILI9341::Status::Ok)
        {
            return s;
        }
    }

    if (param && paramSize)
    {
        this->hal_.SetGpioLevel(this->config_.dcGpio, this->config_.flags.dcParamLevel);
        return this->hal_.SpiWrite(static_cast<const uint8_t *>(param), paramSize);
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::SpiIo::TxColor(int cmd, const void *color, size_t colorSize)
{
    ILI9341::Status s = this->TxParam(cmd, nullptr, 0);
    if (s != ILI9341::Status::Ok)
    {
        return s;
    }

    this->hal_.SetGpioLevel(this->config_.dcGpio, this->config_.flags.dcDataLevel);
    const uint8_t *p = static_cast<const uint8_t *>(color);
    size_t remaining = colorSize;
    while (remaining > 0)
    {
        size_t chunk  = remaining < this->config_.maxChunkBytes ? remaining : this->config_.maxChunkBytes;
        bool   isLast = (chunk == remaining);
        s = this->hal_.SpiWriteAsync(p, chunk, isLast);
        if (s != ILI9341::Status::Ok)
        {
            this->hal_.SpiWaitIdle();
            return s;
        }
        p         += chunk;
        remaining -= chunk;
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::SpiIo::RxParam(int cmd, void *param, size_t paramSize)
{
    /* Read-back via SPI is not implemented. The ILI9341 SPI interface
     * requires a dummy clock cycle before read data, and many MCU SPI
     * peripherals do not support this cleanly in half-duplex mode.
     * Use a parallel/MCU-bus interface if register reads are required. */
    (void)cmd;
    (void)param;
    (void)paramSize;
    return ILI9341::Status::ErrorNotSupported;
}