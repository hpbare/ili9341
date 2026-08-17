#ifndef ILI9341_HPP_
#define ILI9341_HPP_
#include <cstdint>
#include "ili9341_types.hpp"

namespace ILI9341 {

class Panel {
public:
    Panel(ILI9341::Io &io, ILI9341::Hal &platform, const ILI9341::Config &config);
    ~Panel();

    Panel(const Panel &) = delete;
    Panel &operator=(const Panel &) = delete;

    /**
     * @brief   Reset LCD panel.
     * @return  ILI9341_OK on success.
     */
    ILI9341::Status Reset();
    /**
     * @brief   Initialize LCD panel.
     * @return  ILI9341_OK on success.
     */
    ILI9341::Status Init();
    /**
     * @brief Draw bitmap on LCD panel.
     * @param[in] xStart   Start pixel index in the target frame buffer, on x-axis (xStart is included).
     * @param[in] yStart   Start pixel index in the target frame buffer, on y-axis (yStart is included).
     * @param[in] xEnd     End pixel index in the target frame buffer, on x-axis (xEnd is not included).
     * @param[in] yEnd     End pixel index in the target frame buffer, on y-axis (yEnd is not included).
     * @param[in] colorData RGB color data that will be dumped to the specific window range.
     * @return  ILI9341_OK on success.
     */
    ILI9341::Status DrawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData);
    /**
     * @brief Mirror the LCD panel on specific axis.
     * @note Combine this function with `SwapXY`, one can realize screen rotatation.
     * @param[in] mirrorX Whether the panel will be mirrored about the x_axis.
     * @param[in] mirrorY Whether the panel will be mirrored about the y_axis.
     * @return
     *          - ILI9341_OK on success.
     *          - ILI9341_ERROR_NOT_SUPPORTED if this function is not supported by the panel.
     */
    ILI9341::Status Mirror(bool mirrorX, bool mirrorY);
    /**
     * @brief Swap/Exchange x and y axis.
     * @note Combine this function with `mirror`, one can realize screen rotatation.
     * @param[in] swapAxes Whether to swap the x and y axis.
     * @return
     *          - ILI9341_OK on success.
     *          - ILI9341_ERROR_NOT_SUPPORTED if this function is not supported by the panel.
     */
    ILI9341::Status SwapXY(bool swapAxes);
    /**
     * @brief Set extra gap in x and y axis.
     * @note The gap is only used for calculating the real coordinates..
     * @param[in] xGap Extra gap on x axis, in pixels.
     * @param[in] yGap Extra gap on y axis, in pixels.
     * @return ILI9341_OK on success.
     */
    ILI9341::Status SetGap(int xGap, int yGap);
    /**
     * @brief Invert the color (bit 1 -> 0 for color data line, and vice versa).
     * @param[in] invert Whether to invert the color data.
     * @return      ILI9341_OK on success.
     */
    ILI9341::Status InvertColor(bool invert);
    /**
     * @brief Turn on or off the display.
     * @param[in] on_off True to turns on display, False to turns off display.
     * @return
     *          - ILI9341_OK on success.
     *          - ILI9341_ERROR_NOT_SUPPORTED if this function is not supported by the panel.
     */
    ILI9341::Status DispOnOff(bool on);
    /**
     * @brief Enter or exit sleep mode.
     * @param[in] sleep True to enter sleep mode, False to wake up.
     * @return
     *          - ILI9341_OK on success.
     *          - ILI9341_ERROR_NOT_SUPPORTED if this function is not supported by the panel.
     */
    ILI9341::Status DispSleep(bool sleep);
private:
    ILI9341::Io     &io_;
    ILI9341::Hal    &platform_;
    ILI9341::Config config_;

    int             xGap_           = 0;
    int             yGap_           = 0;
    uint8_t         fbBitsPerPixel_ = 16;
    uint8_t         madctlVal_      = 0;    // save current value of LCD_CMD_MADCTL register /* = uint8_t madctl_val; */
    uint8_t         colmodVal_      = 0;    // save current value of LCD_CMD_COLMOD /* = uint8_t colmod_val; */
};

}

#endif /* ILI9341_HPP_ */