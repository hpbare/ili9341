#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_

#include <cstdint>
#include "ili9341.hpp"

/**
 * @file    display.hpp
 * @brief   High-level display wrapper around ILI9341::Panel.
 *
 * Provides primitive drawing operations (pixel, line, rect, fill)
 * using RGB565 (16-bit) colour, and hardware-assisted rotation via
 * ILI9341 MADCTL.  The class owns a frame-buffer slice on the stack
 * only during DrawLine/FillRect operations – no heap allocation.
 *
 * Coordinate system
 * -----------------
 *   (0,0) is the top-left corner.
 *   x grows to the right, y grows downward.
 *   All coordinates are in logical pixels after rotation is applied.
 *
 * Usage example
 * -------------
 * @code
 *   Display disp(panel, 240, 320);
 *   disp.SetRotation(Display::Rotation::Landscape);
 *   disp.FillScreen(Display::Color::Black);
 *   disp.DrawRect(10, 10, 100, 50, Display::Color::White);
 * @endcode
 */

class Display
{
public:
    /** @brief Pre-defined RGB565 colour constants. */
    struct Color
    {
        static constexpr uint16_t Black   = 0x0000;
        static constexpr uint16_t White   = 0xFFFF;
        static constexpr uint16_t Red     = 0xF800;
        static constexpr uint16_t Green   = 0x07E0;
        static constexpr uint16_t Blue    = 0x001F;
        static constexpr uint16_t Yellow  = 0xFFE0;
        static constexpr uint16_t Cyan    = 0x07FF;
        static constexpr uint16_t Magenta = 0xF81F;
        static constexpr uint16_t Orange  = 0xFD20;
        static constexpr uint16_t Gray    = 0x8410;

        /**
         * @brief Pack individual R, G, B components into an RGB565 value.
         * @param r  Red   component (0–255).
         * @param g  Green component (0–255).
         * @param b  Blue  component (0–255).
         * @return   Packed RGB565 word (big-endian ready for SPI).
         */
        static constexpr uint16_t From888(uint8_t r, uint8_t g, uint8_t b)
        {
            return static_cast<uint16_t>(((r & 0xF8u) << 8) |
                                         ((g & 0xFCu) << 3) |
                                         ((b & 0xF8u) >> 3));
        }
    };

    /**
     * @brief Screen rotation.
     *
     * Portrait / Landscape are relative to the panel's native orientation
     * (240 wide × 320 tall at Portrait0).
     */
    enum class Rotation : uint8_t
    {
        Portrait0   = 0, /**< 240 × 320, natural orientation.          */
        Landscape90 = 1, /**< 320 × 240, rotated 90 ° clockwise.       */
        Portrait180 = 2, /**< 240 × 320, upside-down.                  */
        Landscape270 = 3,/**< 320 × 240, rotated 270 ° clockwise.      */
    };

    /**
     * @brief Construct a Display wrapper.
     * @param panel         Reference to an initialised ILI9341::Panel.
     * @param nativeWidth   Panel width  in pixels at Rotation::Portrait0.
     * @param nativeHeight  Panel height in pixels at Rotation::Portrait0.
     */
    Display(ILI9341::Panel &panel, int nativeWidth, int nativeHeight);

    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;

    /* ------------------------------------------------------------------ */
    /* Configuration                                                        */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Apply hardware rotation via MADCTL.
     * @param rotation  Desired screen orientation.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status SetRotation(Rotation rotation);

    /** @brief Return current rotation. */
    Rotation GetRotation() const { return rotation_; }

    /** @brief Return logical screen width (accounts for rotation). */
    int Width() const { return width_; }

    /** @brief Return logical screen height (accounts for rotation). */
    int Height() const { return height_; }

    /* ------------------------------------------------------------------ */
    /* Drawing primitives                                                   */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Fill the entire screen with a single colour.
     * @param color  RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status FillScreen(uint16_t color);

    /**
     * @brief Clear the entire screen to black.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status Clear();

    /**
     * @brief Draw a single pixel.
     * @param x      Pixel x-coordinate (0-based).
     * @param y      Pixel y-coordinate (0-based).
     * @param color  RGB565 colour value.
     * @return ILI9341::Status::Ok on success, or ErrorInvalidArg if out of bounds.
     */
    ILI9341::Status DrawPixel(int x, int y, uint16_t color);

    /**
     * @brief Draw a horizontal line.
     * @param x      Start x-coordinate.
     * @param y      y-coordinate (constant).
     * @param width  Length of the line in pixels.
     * @param color  RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status DrawHLine(int x, int y, int width, uint16_t color);

    /**
     * @brief Draw a vertical line.
     * @param x       x-coordinate (constant).
     * @param y       Start y-coordinate.
     * @param height  Length of the line in pixels.
     * @param color   RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status DrawVLine(int x, int y, int height, uint16_t color);

    /**
     * @brief Draw a straight line between two arbitrary points
     *        using Bresenham's algorithm.
     * @param x0, y0  Start point.
     * @param x1, y1  End point.
     * @param color   RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status DrawLine(int x0, int y0, int x1, int y1, uint16_t color);

    /**
     * @brief Draw an axis-aligned rectangle (outline only).
     * @param x, y    Top-left corner.
     * @param w, h    Width and height in pixels.
     * @param color   RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status DrawRect(int x, int y, int w, int h, uint16_t color);

    /**
     * @brief Draw a filled axis-aligned rectangle.
     * @param x, y    Top-left corner.
     * @param w, h    Width and height in pixels.
     * @param color   RGB565 colour value.
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status FillRect(int x, int y, int w, int h, uint16_t color);

private:
    ILI9341::Panel &panel_;
    int             nativeWidth_;
    int             nativeHeight_;
    int             width_;
    int             height_;
    Rotation        rotation_;

    /**
     * @brief Fill a rectangular window on the panel using a single RGB565 colour.
     *        Pixels are sent in a small tile buffer to avoid large stack allocations.
     */
    ILI9341::Status FillWindow(int xStart, int yStart, int xEnd, int yEnd, uint16_t color);
};

#endif /* DISPLAY_HPP_ */
