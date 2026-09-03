#ifndef DISPLAY_MANAGER_HPP_
#define DISPLAY_MANAGER_HPP_

#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ili9341_types.hpp"
#include "ili9341_io.hpp"
#include "ili9341.hpp"
#include "stm32_hal.hpp"

/* ── Optional LVGL integration ──────────────────────────────────────────── */
#ifdef DISPLAY_MANAGER_USE_LVGL
#include "lvgl.h"
#endif

/**
 * @file    display_manager.hpp
 * @brief   Application-level display manager for ILI9341 on STM32 + FreeRTOS.
 *
 * Responsibilities
 * ----------------
 *  1. Own and wire together Stm32Hal → SpiIo → Panel.
 *  2. Expose Init() to bring the hardware up.
 *  3. Expose Task() as a FreeRTOS task body that drives LVGL
 *     (lv_timer_handler) and flushes the frame buffer to the panel.
 *  4. Provide the LVGL flush callback and tick source (when LVGL is enabled).
 *
 * Typical usage (in freertos.c USER CODE sections)
 * -------------------------------------------------
 * @code
 *   // ── Declaration (file scope) ──
 *   static DisplayManager *gDisplay;
 *
 *   // ── MX_FREERTOS_Init() ──
 *   static DisplayManager::Config cfg;
 *   cfg.panelWidth       = 240;
 *   cfg.panelHeight      = 320;
 *   cfg.rotation         = DisplayManager::Rotation::Landscape90;
 *   cfg.dcPin            = Stm32Hal::EncodePin(ILI9341_DC_GPIO_Port,  ILI9341_DC_Pin);
 *   cfg.rstPin           = Stm32Hal::EncodePin(ILI9341_RST_GPIO_Port, ILI9341_RST_Pin);
 *   cfg.spi              = &hspi1;
 *   cfg.taskStackWords   = 512;
 *   cfg.taskPriority     = osPriorityNormal;
 *
 *   static DisplayManager display(cfg);
 *   gDisplay = &display;
 *   display.Init();
 *
 *   // ── In StartDisplayTask() ──
 *   gDisplay->Task();
 * @endcode
 *
 * LVGL integration
 * ----------------
 * Define DISPLAY_MANAGER_USE_LVGL in your build system (CMake or compiler
 * flags) to compile in the LVGL tick and flush bridge.  When enabled:
 *   - lv_init() is called inside Init().
 *   - lv_tick_inc() is called every 1 ms from an OS timer.
 *   - lv_display_set_flush_cb() is registered; the flush callback DMA's
 *     the dirty rectangle directly through ILI9341::Panel::DrawBitmap().
 */
class DisplayManager
{
public:
    /** @brief Screen rotation (mirrors Display::Rotation concept). */
    enum class Rotation : uint8_t
    {
        Portrait0    = 0,
        Landscape90  = 1,
        Portrait180  = 2,
        Landscape270 = 3,
    };

    /** @brief Full configuration for the display pipeline. */
    struct Config
    {
        /* ── Panel geometry ── */
        int      panelWidth  = 240; /**< Native panel width  (Portrait0). */
        int      panelHeight = 320; /**< Native panel height (Portrait0). */
        Rotation rotation    = Rotation::Portrait0;

        /* ── Hardware pins (use Stm32Hal::EncodePin) ── */
        int dcPin  = -1; /**< Data/Command GPIO (required). */
        int rstPin = -1; /**< Reset GPIO (-1 = not wired).  */

        /* ── SPI ── */
        SPI_HandleTypeDef *spi            = nullptr;
        uint32_t           spiTimeoutMs   = 100;

        /* ── Colour format ── */
        ILI9341::RgbElementOrder rgbOrder    = ILI9341::RgbElementOrder::Rgb;
        ILI9341::BitsPerPixel    bitsPerPixel = ILI9341::BitsPerPixel::Bpp16;

        /* ── FreeRTOS task ── */
        uint32_t    taskStackWords = 512;   /**< Stack in 32-bit words.   */
        UBaseType_t taskPriority   = tskIDLE_PRIORITY + 1;

#ifdef DISPLAY_MANAGER_USE_LVGL
        /* ── LVGL ── */
        /**
         * Size of the LVGL draw buffer in pixels.
         * Recommended: at least panelWidth * 10 (two-buffer DMA flush).
         * Set to 0 to use the default (panelWidth * 10).
         */
        uint32_t lvglBufPixels = 0;
#endif
    };

    /* ------------------------------------------------------------------ */

    explicit DisplayManager(const Config &cfg);
    ~DisplayManager();

    DisplayManager(const DisplayManager &) = delete;
    DisplayManager &operator=(const DisplayManager &) = delete;

    /* ------------------------------------------------------------------ */
    /* Lifecycle                                                            */
    /* ------------------------------------------------------------------ */

    /**
     * @brief  Initialise the hardware and (optionally) LVGL.
     *
     * Must be called once before Task() is started.
     * Calls Panel::Reset() → Panel::Init() → applies rotation.
     *
     * @return ILI9341::Status::Ok on success.
     */
    ILI9341::Status Init();

    /**
     * @brief  FreeRTOS task body — never returns.
     *
     * Without LVGL: empty infinite loop (extend as needed).
     * With    LVGL: calls lv_timer_handler() and yields to the scheduler.
     *
     * Call this from your display task function:
     * @code
     *   void StartDisplayTask(void *arg) { gDisplay->Task(); }
     * @endcode
     */
    [[noreturn]] void Task();

    /* ------------------------------------------------------------------ */
    /* Accessors                                                            */
    /* ------------------------------------------------------------------ */

    /** @brief Direct access to the underlying ILI9341 panel driver. */
    ILI9341::Panel &GetPanel() { return panel_; }

    /** @brief Direct access to the STM32 HAL adapter. */
    Stm32Hal &GetHal() { return hal_; }

    /** @brief Logical display width after rotation. */
    int Width()  const { return logicalWidth_; }

    /** @brief Logical display height after rotation. */
    int Height() const { return logicalHeight_; }

    /* ------------------------------------------------------------------ */
    /* ISR / DMA callback relay                                             */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Forward HAL_SPI_TxCpltCallback() here.
     *
     * Wire in stm32f4xx_it.c or wherever HAL_SPI_TxCpltCallback() lives:
     * @code
     *   void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
     *       if (hspi->Instance == SPI1) gDisplay->OnSpiTxComplete();
     *   }
     * @endcode
     */
    void OnSpiTxComplete();

private:
    /* ── Hardware objects (order matters – hal_ must be first) ── */
    Config          cfg_;
    Stm32Hal        hal_;
    ILI9341::SpiIoConfig spiIoCfg_;
    ILI9341::SpiIo  spiIo_;
    ILI9341::Config panelCfg_;
    ILI9341::Panel  panel_;

    /* ── Logical dimensions (post-rotation) ── */
    int logicalWidth_  = 0;
    int logicalHeight_ = 0;

    /* ── Helpers ── */
    void ApplyRotation(Rotation rotation);

#ifdef DISPLAY_MANAGER_USE_LVGL
    /* ── LVGL ── */
    lv_display_t *lvDisplay_  = nullptr;
    lv_color_t   *lvBuf1_     = nullptr;
    lv_color_t   *lvBuf2_     = nullptr;
    uint32_t      lvBufPixels_ = 0;

    /** LVGL flush callback (static trampoline → instance method). */
    static void LvFlushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

    /** LVGL tick callback (called by lv_tick_get). */
    static uint32_t LvTickCb();

    /** Per-instance flush implementation. */
    void DoFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

    /** Semaphore released by OnSpiTxComplete() to signal flush done. */
    SemaphoreHandle_t flushSem_ = nullptr;

    /** Static trampoline forwarded from Hal::TransDoneCallback. */
    static void OnTransDone(void *ctx);
#endif
};

#endif /* DISPLAY_MANAGER_HPP_ */
