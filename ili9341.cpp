#include "ili9341.hpp"
#include "ili9341_types.hpp"
#include "ili9341_cmds.hpp"

#ifdef ILI9341_DEBUG
#include <iostream>
#endif

namespace
{

    // Default vendor init sequence for ILI9341, used when Config::initCmds is
    // null. Internal linkage (anonymous namespace) - not part of the public API.

    constexpr uint8_t kPowerCtrlB[]         = {0x00, 0xAA, 0xE0};
    constexpr uint8_t kPowerOnSeqCtrl[]     = {0x67, 0x03, 0x12, 0x81};
    constexpr uint8_t kDriverTimingCtrlA[]  = {0x8A, 0x01, 0x78};
    constexpr uint8_t kPowerCtrlA[]         = {0x39, 0x2C, 0x00, 0x34, 0x02};
    constexpr uint8_t kPumpRatioCtrl[]      = {0x20};
    constexpr uint8_t kDriverTimingCtrl[]   = {0x00, 0x00};
    constexpr uint8_t kPowerCtrl1[]         = {0x23};
    constexpr uint8_t kPowerCtrl2[]         = {0x11};
    constexpr uint8_t kVcomCtrl1[]          = {0x43, 0x4C};
    constexpr uint8_t kVcomCtrl2[]          = {0xA0};
    constexpr uint8_t kFrameRateCtrl[]      = {0x00, 0x1B};
    constexpr uint8_t kEnable3G[]           = {0x00};
    constexpr uint8_t kGammaSet[]           = {0x01};
    constexpr uint8_t kPositiveGamma[]      = {0x1F, 0x36, 0x36, 0x3A, 0x0C, 0x05, 0x4F, 0x87, 0x3C, 0x08, 0x11, 0x35, 0x19, 0x13, 0x00};
    constexpr uint8_t kNegativeGamma[]      = {0x00, 0x09, 0x09, 0x05, 0x13, 0x0A, 0x30, 0x78, 0x43, 0x07, 0x0E, 0x0A, 0x26, 0x2C, 0x1F};
    constexpr uint8_t kEntryModeSet[]       = {0x07};
    constexpr uint8_t kDisplayFuncCtrl[]    = {0x08, 0x82, 0x27};

    constexpr ILI9341::InitCommand kDefaultInitCmds[] = {
        // {cmd, data, data_bytes, delay_ms}
        {0xCF, kPowerCtrlB, sizeof(kPowerCtrlB), 0},               // Power control B
        {0xED, kPowerOnSeqCtrl, sizeof(kPowerOnSeqCtrl), 0},       // Power on sequence control
        {0xE8, kDriverTimingCtrlA, sizeof(kDriverTimingCtrlA), 0}, // Driver timing control A
        {0xCB, kPowerCtrlA, sizeof(kPowerCtrlA), 0},               // Power control A
        {0xF7, kPumpRatioCtrl, sizeof(kPumpRatioCtrl), 0},         // Pump ratio control
        {0xEA, kDriverTimingCtrl, sizeof(kDriverTimingCtrl), 0},   // Driver timing control
        {0xC0, kPowerCtrl1, sizeof(kPowerCtrl1), 0},               // Power control 1
        {0xC1, kPowerCtrl2, sizeof(kPowerCtrl2), 0},               // Power control 2
        {0xC5, kVcomCtrl1, sizeof(kVcomCtrl1), 0},                 // VCOM control 1
        {0xC7, kVcomCtrl2, sizeof(kVcomCtrl2), 0},                 // VCOM control 2
        {0xB1, kFrameRateCtrl, sizeof(kFrameRateCtrl), 0},         // Frame rate control
        {0xF2, kEnable3G, sizeof(kEnable3G), 0},                   // Enable 3G, disabled
        {0x26, kGammaSet, sizeof(kGammaSet), 0},                   // Gamma set, curve 1
        {0xE0, kPositiveGamma, sizeof(kPositiveGamma), 0},         // Positive gamma correction
        {0xE1, kNegativeGamma, sizeof(kNegativeGamma), 0},         // Negative gamma correction
        {0xB7, kEntryModeSet, sizeof(kEntryModeSet), 0},           // Entry mode set
        {0xB6, kDisplayFuncCtrl, sizeof(kDisplayFuncCtrl), 0},     // Display function control
    };

    constexpr size_t kDefaultInitCmdsSize = sizeof(kDefaultInitCmds) / sizeof(kDefaultInitCmds[0]);

}

ILI9341::Panel::Panel(ILI9341::Io &io, ILI9341::Hal &platform, const ILI9341::Config &config)
    : io_(io), platform_(platform), config_(config)
{
    // Configure reset pin as output, if wired (mirrors gpio_config() in the
    // original factory function). Panel owns the pin number/level; platform_
    // just executes the primitive.
    if (this->config_.resetGpioNum >= 0) {
        platform_.ConfigureOutputPin(this->config_.resetGpioNum);
    }

    // RGB/BGR element order -> MADCTL BGR bit
    madctlVal_ = (config_.rgbOrder == RgbElementOrder::Bgr) ? ILI9341::Madctl::BGR : 0;

    // Pixel format -> COLMOD value + framebuffer bit depth
    switch (config_.bitsPerPixel) {
    case BitsPerPixel::Bpp16:
        colmodVal_      = 0x55;
        fbBitsPerPixel_ = 16;
        break;
    case BitsPerPixel::Bpp18:
        colmodVal_      = 0x66;
        fbBitsPerPixel_ = 24; // each R/G/B component occupies a full byte (6 high bits used)
        break;
    // default:
    //     return ILI9341::Status::ErrorInvalidColorFormat
    }
}

ILI9341::Panel::~Panel()
{
    if (this->config_.resetGpioNum >= 0) {
        platform_.ReleasePin(this->config_.resetGpioNum);
    }
}

ILI9341::Status ILI9341::Panel::Reset(void){
    if(this->config_.resetGpioNum >= 0){
        this->platform_.SetGpioLevel(this->config_.resetGpioNum, this->config_.flags.resetActiveHigh);
        this->platform_.DelayMs(10);
        this->platform_.SetGpioLevel(this->config_.resetGpioNum, !this->config_.flags.resetActiveHigh);
        this->platform_.DelayMs(10);
    } else {
        ILI9341::Status s = this->io_.TxParam(ILI9341::Cmd::SWRESET, nullptr, 0);
        if(s != ILI9341::Status::Ok){
            return s;   
        }
        this->platform_.DelayMs(20);
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::Panel::Init(void){
    ILI9341::Status s = ILI9341::Status::Ok;

    s = this->io_.TxParam(ILI9341::Cmd::SLPOUT, nullptr, 0);
    if(s != ILI9341::Status::Ok){
        return s;
    }
    this->platform_.DelayMs(100);

    uint8_t madctl_param[] = { this->madctlVal_ };
    s = this->io_.TxParam(ILI9341::Cmd::MADCTL, madctl_param, sizeof(madctl_param));
    if(s != ILI9341::Status::Ok){
        return s;
    }

    uint8_t colmod_param[] = { this->colmodVal_ };
    s = this->io_.TxParam(ILI9341::Cmd::COLMOD, colmod_param, sizeof(colmod_param));
    if(s != ILI9341::Status::Ok){
        return s;
    }

    const ILI9341::InitCommand * cmds = NULL;
    uint16_t cmdsSize = 0;
    if(this->config_.initCmds){
        cmds     = this->config_.initCmds;
        cmdsSize = this->config_.initCmdsSize;
    } else {
        cmds     = kDefaultInitCmds;
        cmdsSize = kDefaultInitCmdsSize;
    }

    #ifdef ILI9341_DEBUG
    bool isCmdOverwritten = false;
    #endif /* ILI9341_DEBUG */
    for (uint16_t i = 0; i < cmdsSize; i++)
    {
        switch (cmds[i].cmd)
        {
        case ILI9341::Cmd::MADCTL:
            #ifdef ILI9341_DEBUG
            isCmdOverwritten = true;
            #endif /* ILI9341_DEBUG */
            if(cmds[i].data != nullptr && cmds[i].data_bytes > 0){
                this->madctlVal_ = static_cast<const uint8_t *>(cmds[i].data)[0];
            }
            break;

        case ILI9341::Cmd::COLMOD:
            #ifdef ILI9341_DEBUG
            isCmdOverwritten = true;
            #endif /* ILI9341_DEBUG */
            if(cmds[i].data != nullptr && cmds[i].data_bytes > 0){
                this->colmodVal_ = static_cast<const uint8_t *>(cmds[i].data)[0];
            }
            break;

        default:
            #ifdef ILI9341_DEBUG
            isCmdOverwritten = false;
            #endif /* ILI9341_DEBUG */
            break;
        }

        #ifdef ILI9341_DEBUG
        if(isCmdOverwritten){
            std::cout << "[ILI9341]:\tThe " << std::hex << static_cast<int>(cmds[i].cmd) << " command has been used and will be overwritten by external initialization sequence." << std::endl;
            // printf("[ILI9341]:\tThe %02Xh command has been used and will be overwritten by external initialization sequence.", cmds[i].cmd);
        }
        #endif /* ILI9341_DEBUG */

        s = this->io_.TxParam(cmds[i].cmd, cmds[i].data, cmds[i].data_bytes);
        if(s != ILI9341::Status::Ok){
            return s;
        }

        this->platform_.DelayMs(cmds[i].delay_ms);
    }

    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::Panel::DrawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData){
    if(xStart >= xEnd || yStart >= yEnd || colorData == nullptr){
        return ILI9341::Status::ErrorInvalidArg;
    }

    xStart += this->xGap_;
    xEnd   += this->xGap_;
    yStart += this->yGap_;
    yEnd   += this->yGap_;

    ILI9341::Status s = ILI9341::Status::Ok;
    uint8_t caset_param[] = {
        static_cast<uint8_t>((xStart >> 8)      & 0xFF),
        static_cast<uint8_t>((xStart)           & 0xFF),
        static_cast<uint8_t>(((xEnd - 1) >> 8)  & 0xFF),
        static_cast<uint8_t>((xEnd - 1)         & 0xFF)
    };
    size_t caset_param_size = sizeof(caset_param);
    s = this->io_.TxParam(ILI9341::Cmd::CASET, caset_param, caset_param_size);
    if(s != ILI9341::Status::Ok){
        return s;
    }

    uint8_t raset_param[] = {
        static_cast<uint8_t>((yStart >> 8)      & 0xFF),
        static_cast<uint8_t>((yStart)           & 0xFF),
        static_cast<uint8_t>(((yEnd - 1) >> 8)  & 0xFF),
        static_cast<uint8_t>((yEnd - 1)         & 0xFF)
    };
    size_t raset_param_size = sizeof(raset_param);
    s = this->io_.TxParam(ILI9341::Cmd::RASET, raset_param, raset_param_size);
    if(s != ILI9341::Status::Ok){
        return s;
    }
    /* Transfer frame buffer */
    size_t len = (xEnd - xStart) * (yEnd - yStart) * this->fbBitsPerPixel_ / 8;

    s = this->io_.TxColor(ILI9341::Cmd::RAMWR, colorData, len);
    return s;
}

ILI9341::Status ILI9341::Panel::InvertColor(bool invert){
    int cmd = invert ? ILI9341::Cmd::INVON : ILI9341::Cmd::INVOFF;
    ILI9341::Status s = this->io_.TxParam(cmd, nullptr, 0);
    return s;
}

ILI9341::Status ILI9341::Panel::Mirror(bool mirrorX, bool mirrorY){
    if(mirrorX){
        this->madctlVal_ |= ILI9341::Madctl::MX;
    } else {
        this->madctlVal_ &= ~ILI9341::Madctl::MX;
    }

    if(mirrorY){
        this->madctlVal_ |= ILI9341::Madctl::MY;
    } else {
        this->madctlVal_ &= ~ILI9341::Madctl::MY;
    }

    uint8_t param[] = { this->madctlVal_ };
    ILI9341::Status s = this->io_.TxParam(ILI9341::Cmd::MADCTL, param, sizeof(param));

    return s;
}

ILI9341::Status ILI9341::Panel::SwapXY(bool swapAxes){
    if(swapAxes){
        this->madctlVal_ |= ILI9341::Madctl::MV;
    } else {
        this->madctlVal_ &= ~ILI9341::Madctl::MV;
    }

    uint8_t param[] = { this->madctlVal_ };
    ILI9341::Status s = this->io_.TxParam(ILI9341::Cmd::MADCTL, param, sizeof(param));

    return s;
}

ILI9341::Status ILI9341::Panel::SetGap(int xGap, int yGap){
    this->xGap_ = xGap;
    this->yGap_ = yGap;
    return ILI9341::Status::Ok;
}

ILI9341::Status ILI9341::Panel::DispOnOff(bool on){
    int cmd = on ? ILI9341::Cmd::DISPON : ILI9341::Cmd::DISPOFF;
    ILI9341::Status s = this->io_.TxParam(cmd, nullptr, 0);
    return s;
}

ILI9341::Status ILI9341::Panel::DispSleep(bool sleep)
{
    int cmd = sleep ? ILI9341::Cmd::SLPIN : ILI9341::Cmd::SLPOUT;
    ILI9341::Status s = this->io_.TxParam(cmd, nullptr, 0);
    if (s != ILI9341::Status::Ok)
    {
        return s;
    }

    this->platform_.DelayMs(sleep ? 120 : 5);
    return ILI9341::Status::Ok;
}