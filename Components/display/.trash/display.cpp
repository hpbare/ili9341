#include "display.hpp"
#include <cstdlib>  // std::abs

/* ── tuneable: tile buffer size (bytes on stack per FillWindow call) ──────── */
static constexpr int kTileBufPixels = 128; /* 256 bytes on stack */

/* =========================================================================
 * Helpers
 * ========================================================================= */

/** @brief Swap two int values in-place. */
static inline void Swap(int &a, int &b) { int t = a; a = b; b = t; }

/**
 * @brief Convert an RGB565 pixel to a big-endian byte pair expected by
 *        the ILI9341 SPI bus (MSB first).
 */
static inline void Rgb565ToBytes(uint16_t color, uint8_t out[2])
{
    out[0] = static_cast<uint8_t>(color >> 8);
    out[1] = static_cast<uint8_t>(color & 0xFF);
}

/* =========================================================================
 * Constructor
 * ========================================================================= */

Display::Display(ILI9341::Panel &panel, int nativeWidth, int nativeHeight)
    : panel_(panel),
      nativeWidth_(nativeWidth),
      nativeHeight_(nativeHeight),
      width_(nativeWidth),
      height_(nativeHeight),
      rotation_(Rotation::Portrait0)
{
}

/* =========================================================================
 * Configuration
 * ========================================================================= */

ILI9341::Status Display::SetRotation(Rotation rotation)
{
    rotation_ = rotation;

    /* Update logical dimensions */
    switch (rotation)
    {
    case Rotation::Portrait0:
    case Rotation::Portrait180:
        width_  = nativeWidth_;
        height_ = nativeHeight_;
        break;
    case Rotation::Landscape90:
    case Rotation::Landscape270:
        width_  = nativeHeight_;
        height_ = nativeWidth_;
        break;
    }

    /*
     * Map rotation to Mirror + SwapXY combinations that implement
     * the four orientations on ILI9341:
     *
     *   Portrait0   (0 °)  : no mirror, no swap
     *   Landscape90 (90°)  : mirrorX,   swap
     *   Portrait180 (180°) : mirrorX + mirrorY, no swap
     *   Landscape270(270°) : mirrorY,   swap
     */
    ILI9341::Status s;

    switch (rotation)
    {
    case Rotation::Portrait0:
        s = panel_.Mirror(false, false);
        if (s != ILI9341::Status::Ok) return s;
        s = panel_.SwapXY(false);
        break;

    case Rotation::Landscape90:
        s = panel_.Mirror(true, false);
        if (s != ILI9341::Status::Ok) return s;
        s = panel_.SwapXY(true);
        break;

    case Rotation::Portrait180:
        s = panel_.Mirror(true, true);
        if (s != ILI9341::Status::Ok) return s;
        s = panel_.SwapXY(false);
        break;

    case Rotation::Landscape270:
        s = panel_.Mirror(false, true);
        if (s != ILI9341::Status::Ok) return s;
        s = panel_.SwapXY(true);
        break;

    default:
        s = ILI9341::Status::ErrorInvalidArg;
        break;
    }

    return s;
}

/* =========================================================================
 * Private: fill a rectangular window with a solid colour
 * ========================================================================= */

ILI9341::Status Display::FillWindow(int xStart, int yStart, int xEnd, int yEnd, uint16_t color)
{
    /* Pre-fill a tile buffer with the colour (big-endian) */
    uint8_t tile[kTileBufPixels * 2];
    for (int i = 0; i < kTileBufPixels; ++i)
    {
        Rgb565ToBytes(color, &tile[i * 2]);
    }

    int totalPixels = (xEnd - xStart) * (yEnd - yStart);
    int sent        = 0;

    while (sent < totalPixels)
    {
        int chunk = totalPixels - sent;
        if (chunk > kTileBufPixels) chunk = kTileBufPixels;

        /* Compute the sub-window for this chunk */
        int pixelOffset = sent;
        int rowLen      = xEnd - xStart;
        int r0          = pixelOffset / rowLen;
        int c0          = pixelOffset % rowLen;
        int r1          = (pixelOffset + chunk - 1) / rowLen;
        int c1          = (pixelOffset + chunk - 1) % rowLen;

        /*
         * For simplicity we send the whole row-band from column xStart to
         * xEnd, not a partial column. When a chunk spans multiple rows we
         * flush row-by-row; single-row chunks send a single narrow slice.
         * In practice kTileBufPixels >= rowLen for typical 240-wide panels,
         * so most FillRect calls take a single DrawBitmap per row.
         */
        if (r0 == r1)
        {
            /* Single (partial) row */
            ILI9341::Status s = panel_.DrawBitmap(
                xStart + c0, yStart + r0,
                xStart + c1 + 1, yStart + r0 + 1,
                tile);
            if (s != ILI9341::Status::Ok) return s;
        }
        else
        {
            /* Spans multiple rows: send the first partial row, then fall
             * back to the loop to handle the remainder. */
            int firstLen = rowLen - c0;
            ILI9341::Status s = panel_.DrawBitmap(
                xStart + c0, yStart + r0,
                xEnd, yStart + r0 + 1,
                tile);
            if (s != ILI9341::Status::Ok) return s;
            sent += firstLen;
            continue;
        }

        sent += chunk;
    }

    return ILI9341::Status::Ok;
}

/* =========================================================================
 * Public drawing API
 * ========================================================================= */

ILI9341::Status Display::FillScreen(uint16_t color)
{
    return FillWindow(0, 0, width_, height_, color);
}

ILI9341::Status Display::Clear()
{
    return FillScreen(Color::Black);
}

ILI9341::Status Display::DrawPixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= width_ || y < 0 || y >= height_)
    {
        return ILI9341::Status::ErrorInvalidArg;
    }

    uint8_t buf[2];
    Rgb565ToBytes(color, buf);
    return panel_.DrawBitmap(x, y, x + 1, y + 1, buf);
}

ILI9341::Status Display::DrawHLine(int x, int y, int width, uint16_t color)
{
    if (x < 0 || y < 0 || y >= height_ || width <= 0) return ILI9341::Status::ErrorInvalidArg;

    int xEnd = x + width;
    if (x >= width_) return ILI9341::Status::Ok;
    if (xEnd > width_) xEnd = width_;

    return FillWindow(x, y, xEnd, y + 1, color);
}

ILI9341::Status Display::DrawVLine(int x, int y, int height, uint16_t color)
{
    if (y < 0 || x < 0 || x >= width_ || height <= 0) return ILI9341::Status::ErrorInvalidArg;

    int yEnd = y + height;
    if (y >= height_) return ILI9341::Status::Ok;
    if (yEnd > height_) yEnd = height_;

    return FillWindow(x, y, x + 1, yEnd, color);
}

ILI9341::Status Display::DrawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    /* Optimise axis-aligned cases */
    if (y0 == y1) return DrawHLine(x0 < x1 ? x0 : x1, y0, std::abs(x1 - x0) + 1, color);
    if (x0 == x1) return DrawVLine(x0, y0 < y1 ? y0 : y1, std::abs(y1 - y0) + 1, color);

    /* Bresenham's line algorithm */
    int dx  =  std::abs(x1 - x0);
    int dy  = -std::abs(y1 - y0);
    int sx  = (x0 < x1) ? 1 : -1;
    int sy  = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    ILI9341::Status s = ILI9341::Status::Ok;
    while (true)
    {
        s = DrawPixel(x0, y0, color);
        if (s != ILI9341::Status::Ok) return s;

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { if (x0 == x1) break; err += dy; x0 += sx; }
        if (e2 <= dx) { if (y0 == y1) break; err += dx; y0 += sy; }
    }

    return s;
}

ILI9341::Status Display::DrawRect(int x, int y, int w, int h, uint16_t color)
{
    ILI9341::Status s;

    s = DrawHLine(x, y, w, color);           if (s != ILI9341::Status::Ok) return s;
    s = DrawHLine(x, y + h - 1, w, color);  if (s != ILI9341::Status::Ok) return s;
    s = DrawVLine(x, y, h, color);           if (s != ILI9341::Status::Ok) return s;
    s = DrawVLine(x + w - 1, y, h, color);

    return s;
}

ILI9341::Status Display::FillRect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return ILI9341::Status::Ok;

    /* Clamp to screen */
    int xEnd = x + w;
    int yEnd = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (xEnd > width_)  xEnd = width_;
    if (yEnd > height_) yEnd = height_;
    if (x >= xEnd || y >= yEnd) return ILI9341::Status::Ok;

    return FillWindow(x, y, xEnd, yEnd, color);
}
