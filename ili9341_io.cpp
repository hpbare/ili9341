#include "ili9341_io.hpp"

constexpr unsigned int kDefaultDcCmdLevel = 0;

ILI9341::SpiIo::SpiIo(ILI9341::Hal &hal, const ILI9341::SpiIoConfig &config)
    : hal_(hal), config_(config)
{
    if (this->config_.maxChunkBytes == 0)
    {
        this->config_.maxChunkBytes = this->hal_.GetMaxTransferSize();
    }
    this->flags_.dcCmdLevel   = this->config_.flags.dcCmdLevel   ? 1 : 0;
    this->flags_.dcParamLevel = this->config_.flags.dcParamLevel ? 1 : 0;
    this->flags_.dcDataLevel  = this->config_.flags.dcDataLevel  ? 1 : 0;
}

ILI9341::Status ILI9341::SpiIo::TxParam(int cmd, const void *param, size_t paramSize)
{
    if (cmd >= 0)
    {
        this->hal_.SetGpioLevel(this->config_.dcGpio, this->flags_.dcCmdLevel);
        uint8_t cmdByte = static_cast<uint8_t>(cmd);
        ILI9341::Status s = this->hal_.SpiWrite(&cmdByte, 1);
        if (s != ILI9341::Status::Ok)
        {
            return s;
        }
    }

    if (param && paramSize)
    {
        this->hal_.SetGpioLevel(this->config_.dcGpio, this->flags_.dcParamLevel);
        return this->hal_.SpiWrite(static_cast<const uint8_t *>(param), paramSize);
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::SpiIo::TxColor(int cmd, const void *color, size_t colorSize)
{
    ILI9341::Status s = TxParam(cmd, nullptr, 0);
    if (s != ILI9341::Status::Ok)
    {
        return s;
    }

    this->hal_.SetGpioLevel(this->config_.dcGpio, this->flags_.dcDataLevel);
    const uint8_t *p = static_cast<const uint8_t *>(color);
    size_t remaining = colorSize;
    while (remaining > 0)
    {
        size_t chunk = remaining < this->config_.maxChunkBytes ? remaining : this->config_.maxChunkBytes;
        s = this->hal_.SpiWriteAsync(p, chunk);
        if (s != ILI9341::Status::Ok)
        {
            this->hal_.SpiWaitIdle();
            return s;
        }
        p += chunk;
        remaining -= chunk;
    }

    return this->hal_.SpiWaitIdle();
}

ILI9341::Status ILI9341::SpiIo::RxParam(int cmd, void *param, size_t paramSize)
{
    return ILI9341::Status::ErrorNotSupported;
}