#ifndef ILI9341_IO_HPP_
#define ILI9341_IO_HPP_
#include "ili9341_types.hpp"

namespace ILI9341
{
    class SpiIo : public Io
    {
    public:
        SpiIo(Hal &hal, const SpiIoConfig &config);
        ILI9341::Status TxParam(int cmd, const void *param, size_t paramSize) override;
        ILI9341::Status TxColor(int cmd, const void *color, size_t colorSize) override;
        ILI9341::Status RxParam(int cmd, void *param, size_t paramSize) override;

    private:
        Hal &hal_;
        SpiIoConfig config_;
        struct
        {
            unsigned int dcCmdLevel : 1;   // Indicates the level of DC line when transferring command
            unsigned int dcDataLevel : 1;  // Indicates the level of DC line when transferring color data
            unsigned int dcParamLevel : 1; // Indicates the level of DC line when transferring parameters
        } flags_;
    };

}

#endif /* ILI9341_IO_HPP_ */