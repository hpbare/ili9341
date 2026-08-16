#ifndef ILI9341_CMDS_HPP_
#define ILI9341_CMDS_HPP_
#include <cstdint>

namespace ILI9341
{
    /* Common LCD panel commands */
    namespace Cmd
    {
        constexpr uint8_t NOP       = 0x00;   // This command is empty command
        constexpr uint8_t SWRESET   = 0x01;   // Software reset registers (the built-in frame buffer is not affected)
        constexpr uint8_t RDDID     = 0x04;   // Read 24-bit display ID
        constexpr uint8_t RDDST     = 0x09;   // Read display status
        constexpr uint8_t RDDPM     = 0x0A;   // Read display power mode
        constexpr uint8_t RDD_MAD   = 0x0B;   // Read display MADCTL
        constexpr uint8_t RDD_COL   = 0x0C;   // Read display pixel format
        constexpr uint8_t RDDIM     = 0x0D;   // Read display image mode
        constexpr uint8_t RDDSM     = 0x0E;   // Read display signal mode
        constexpr uint8_t RDDSR     = 0x0F;   // Read display self-diagnostic result
        constexpr uint8_t SLPIN     = 0x10;   // Go into sleep mode (DC/DC, oscillator, scanning stopped, but memory keeps content)
        constexpr uint8_t SLPOUT    = 0x11;   // Exit sleep mode
        constexpr uint8_t PTLON     = 0x12;   // Turns on partial display mode
        constexpr uint8_t NORON     = 0x13;   // Turns on normal display mode
        constexpr uint8_t INVOFF    = 0x20;   // Recover from display inversion mode
        constexpr uint8_t INVON     = 0x21;   // Go into display inversion mode
        constexpr uint8_t GAMSET    = 0x26;   // Select Gamma curve for current display
        constexpr uint8_t DISPOFF   = 0x28;   // Display off (disable frame buffer output)
        constexpr uint8_t DISPON    = 0x29;   // Display on (enable frame buffer output)
        constexpr uint8_t CASET     = 0x2A;   // Set column address
        constexpr uint8_t RASET     = 0x2B;   // Set row address
        constexpr uint8_t RAMWR     = 0x2C;   // Write frame memory
        constexpr uint8_t RAMRD     = 0x2E;   // Read frame memory
        constexpr uint8_t PTLAR     = 0x30;   // Define the partial area
        constexpr uint8_t VSCRDEF   = 0x33;   // Vertical scrolling definition
        constexpr uint8_t TEOFF     = 0x34;   // Turns off tearing effect
        constexpr uint8_t TEON      = 0x35;   // Turns on tearing effect
        constexpr uint8_t MADCTL    = 0x36;   // Memory data access control
        constexpr uint8_t VSCSAD    = 0x37;   // Vertical scroll start address
        constexpr uint8_t IDMOFF    = 0x38;   // Recover from IDLE mode
        constexpr uint8_t IDMON     = 0x39;   // Fall into IDLE mode (8 color depth is displayed)
        constexpr uint8_t COLMOD    = 0x3A;   // Defines the format of RGB picture data
        constexpr uint8_t RAMWRC    = 0x3C;   // Memory write continue
        constexpr uint8_t RAMRDC    = 0x3E;   // Memory read continue
        constexpr uint8_t STE       = 0x44;   // Set tear scan line, tearing effect output signal when display module reaches line N
        constexpr uint8_t GDCAN     = 0x45;   // Get scan line
        constexpr uint8_t WRDISBV   = 0x51;   // Write display brightness
        constexpr uint8_t RDDISBV   = 0x52;   // Read display brightness value
    }

    namespace Madctl {
        constexpr uint8_t MH  = 0x04;
        constexpr uint8_t BGR = 0x08;
        constexpr uint8_t ML  = 0x10;
        constexpr uint8_t MV  = 0x20;
        constexpr uint8_t MX  = 0x40;
        constexpr uint8_t MY  = 0x80;
    }
}

#endif /* ILI9341_CMDS_HPP_ */