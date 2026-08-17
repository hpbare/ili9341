#include "ili9341_io.hpp"

constexpr unsigned int kDefaultDcCmdLevel = 0;

ILI9341::SpiIo::SpiIo(ILI9341::Hal &hal, const ILI9341::SpiIoConfig &config)
    : hal_(hal), config_(config)
{
    if (this->config_.maxChunkBytes == 0)
    {
        this->config_.maxChunkBytes = this->hal_.GetMaxTransferSize();
    }
    this->flags_.dcCmdLevel   = this->config_.flags.dcHighOnCmd  ? 1 : 0;
    this->flags_.dcParamLevel = this->config_.flags.dcLowOnParam ? 0 : 1;
    this->flags_.dcDataLevel  = this->config_.flags.dcLowOnData  ? 0 : 1;
}

ILI9341::Status ILI9341::SpiIo::TxParam(int cmd, const void *param, size_t paramSize)
{
    if (cmd >= 0)
    {
        this->hal_.SetGpioLevel(this->config_.dcGpio, flags_.dcCmdLevel);
        uint8_t cmdByte = static_cast<uint8_t>(cmd);
        ILI9341::Status s = hal_.SpiWrite(&cmdByte, 1);
        if (s != ILI9341::Status::Ok)
        {
            return s;
        }
    }

    if (param && paramSize)
    {
        hal_.SetGpioLevel(this->config_.dcGpio, flags_.dcParamLevel);
        return hal_.SpiWrite(static_cast<const uint8_t *>(param), paramSize);
    }

    return ILI9341::Status::Ok;
}

/* Polling pattern */
// ILI9341::Status ILI9341::SpiIo::TxColor(int cmd, const void *color, size_t colorSize)
// {
//     ILI9341::Status s = TxParam(cmd, nullptr, 0);
//     if (s != ILI9341::Status::Ok)
//     {
//         return s;
//     }

//     hal_.SetGpioLevel(this->config_.dcGpio, flags_.dcDataLevel);
//     const uint8_t *p = static_cast<const uint8_t *>(color);
//     size_t remaining = colorSize;
//     while (remaining > 0)
//     {
//         size_t chunk = remaining < this->config_.maxChunkBytes ? remaining : this->config_.maxChunkBytes;
//         s = hal_.SpiWrite(p, chunk);
//         if (s != ILI9341::Status::Ok)
//         {
//             return s;
//         }
//         p += chunk;
//         remaining -= chunk;
//     }
//     return ILI9341::Status::Ok;
// }

/* Queue + ISR pattern */

ILI9341::Status ILI9341::SpiIo::TxColor(int cmd, const void *color, size_t colorSize)
{
    ILI9341::Status s = TxParam(cmd, nullptr, 0);
    if (s != ILI9341::Status::Ok)
    {
        return s;
    }

    hal_.SetGpioLevel(this->config_.dcGpio, flags_.dcDataLevel);
    const uint8_t *p = static_cast<const uint8_t *>(color);
    size_t remaining = colorSize;
    while (remaining > 0)
    {
        size_t chunk = remaining < this->config_.maxChunkBytes ? remaining : this->config_.maxChunkBytes;
        s = hal_.SpiWriteAsync(p, chunk);
        if (s != ILI9341::Status::Ok)
        {
            this->hal_.SpiWaitIdle();
            return s;
        }
        p += chunk;
        remaining -= chunk;
    }

    this->hal_.SpiWaitIdle();
    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::SpiIo::RxParam(int cmd, void *param, size_t paramSize)
{
    return ILI9341::Status::ErrorNotSupported;
}