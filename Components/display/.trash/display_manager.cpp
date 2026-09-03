#include "display_manager.hpp"
#include "cmsis_os.h"   /* osDelay */

/* =========================================================================
 * Constructor / Destructor
 * ========================================================================= */

/*
 * Member initialisation order must match declaration order in the header.
 * hal_ → spiIo_ needs hal_ alive; panel_ needs spiIo_ and hal_ alive.
 */
DisplayManager::DisplayManager(const Config &cfg)
    : cfg_(cfg),
      hal_(cfg.spi, cfg.spiTimeoutMs),
      spiIoCfg_([&cfg]() -> ILI9341::SpiIoConfig {
          ILI9341::SpiIoConfig c;
          c.dcGpio = cfg.dcPin;
          return c;
      }()),
      spiIo_(hal_, spiIoCfg_),
      panelCfg_([&cfg]() -> ILI9341::Config {
          ILI9341::Config c;
          c.resetGpioNum  = cfg.rstPin;
          c.rgbOrder      = cfg.rgbOrder;
          c.bitsPerPixel  = cfg.bitsPerPixel;
          return c;
      }()),
      panel_(spiIo_, hal_, panelCfg_)
{
    /* Compute logical size at construction time */
    logicalWidth_  = cfg_.panelWidth;
    logicalHeight_ = cfg_.panelHeight;
}

DisplayManager::~DisplayManager()
{
#ifdef DISPLAY_MANAGER_USE_LVGL
    if (flushSem_) vSemaphoreDelete(flushSem_);
    vPortFree(lvBuf1_);
    vPortFree(lvBuf2_);
#endif
}

/* =========================================================================
 * Init
 * ========================================================================= */

ILI9341::Status DisplayManager::Init()
{
    /* 1. Hardware reset */
    ILI9341::Status s = panel_.Reset();
    if (s != ILI9341::Status::Ok) return s;

    /* 2. Panel init sequence */
    s = panel_.Init();
    if (s != ILI9341::Status::Ok) return s;

    /* 3. Apply rotation */
    ApplyRotation(cfg_.rotation);

    /* 4. Turn display on */
    s = panel_.DispOnOff(true);
    if (s != ILI9341::Status::Ok) return s;

#ifdef DISPLAY_MANAGER_USE_LVGL
    /* 5. LVGL initialisation -------------------------------------------- */
    lv_init();

    /* Buffer size */
    lvBufPixels_ = cfg_.lvglBufPixels;
    if (lvBufPixels_ == 0)
    {
        lvBufPixels_ = static_cast<uint32_t>(logicalWidth_) * 10u;
    }

    lvBuf1_ = static_cast<lv_color_t *>(pvPortMalloc(lvBufPixels_ * sizeof(lv_color_t)));
    lvBuf2_ = static_cast<lv_color_t *>(pvPortMalloc(lvBufPixels_ * sizeof(lv_color_t)));
    /* lvBuf2_ may be NULL – LVGL works with a single buffer too */

    /* Create LVGL display object */
    lvDisplay_ = lv_display_create(logicalWidth_, logicalHeight_);
    lv_display_set_buffers(lvDisplay_,
                           lvBuf1_,
                           lvBuf2_,
                           lvBufPixels_ * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(lvDisplay_, LvFlushCb);
    lv_display_set_user_data(lvDisplay_, this); /* pass instance to trampoline */

    /* Wire tick source */
    lv_tick_set_cb(LvTickCb);

    /* Semaphore signalled by DMA-done → lv_display_flush_ready */
    flushSem_ = xSemaphoreCreateBinary();

    /* Register DMA-done callback so flush knows when the SPI transfer ends */
    hal_.RegisterTransDoneCallback(OnTransDone, this);
#endif /* DISPLAY_MANAGER_USE_LVGL */

    return ILI9341::Status::Ok;
}

/* =========================================================================
 * Task
 * ========================================================================= */

void DisplayManager::Task()
{
#ifdef DISPLAY_MANAGER_USE_LVGL
    for (;;)
    {
        /*
         * lv_timer_handler() runs all pending LVGL timers (animations,
         * scroll, etc.) and triggers a flush when a dirty region exists.
         * Returns the number of ms until the next timer fires.
         */
        uint32_t sleepMs = lv_timer_handler();
        if (sleepMs > 0)
        {
            osDelay(sleepMs);
        }
    }
#else
    /* No LVGL – placeholder infinite loop */
    for (;;)
    {
        osDelay(portMAX_DELAY);
    }
#endif
}

/* =========================================================================
 * ISR relay
 * ========================================================================= */

void DisplayManager::OnSpiTxComplete()
{
    hal_.OnTxComplete();
}

/* =========================================================================
 * Private helpers
 * ========================================================================= */

void DisplayManager::ApplyRotation(Rotation rotation)
{
    switch (rotation)
    {
    case Rotation::Portrait0:
        panel_.Mirror(false, false);
        panel_.SwapXY(false);
        logicalWidth_  = cfg_.panelWidth;
        logicalHeight_ = cfg_.panelHeight;
        break;

    case Rotation::Landscape90:
        panel_.Mirror(true, false);
        panel_.SwapXY(true);
        logicalWidth_  = cfg_.panelHeight;
        logicalHeight_ = cfg_.panelWidth;
        break;

    case Rotation::Portrait180:
        panel_.Mirror(true, true);
        panel_.SwapXY(false);
        logicalWidth_  = cfg_.panelWidth;
        logicalHeight_ = cfg_.panelHeight;
        break;

    case Rotation::Landscape270:
        panel_.Mirror(false, true);
        panel_.SwapXY(true);
        logicalWidth_  = cfg_.panelHeight;
        logicalHeight_ = cfg_.panelWidth;
        break;
    }
}

/* =========================================================================
 * LVGL integration (compiled only when DISPLAY_MANAGER_USE_LVGL is defined)
 * ========================================================================= */

#ifdef DISPLAY_MANAGER_USE_LVGL

/* static */ void DisplayManager::LvFlushCb(lv_display_t *disp,
                                             const lv_area_t *area,
                                             uint8_t *px_map)
{
    auto *self = static_cast<DisplayManager *>(lv_display_get_user_data(disp));
    self->DoFlush(disp, area, px_map);
}

/* static */ uint32_t DisplayManager::LvTickCb()
{
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* static */ void DisplayManager::OnTransDone(void *ctx)
{
    auto *self = static_cast<DisplayManager *>(ctx);

    /* Called from ISR (DMA complete) – use FromISR variant */
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(self->flushSem_, &woken);
    portYIELD_FROM_ISR(woken);
}

void DisplayManager::DoFlush(lv_display_t *disp,
                              const lv_area_t *area,
                              uint8_t *px_map)
{
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1; /* DrawBitmap xEnd is exclusive */
    int y2 = area->y2 + 1;

    panel_.DrawBitmap(x1, y1, x2, y2, px_map);

    /*
     * If DMA is configured, DrawBitmap returns before the transfer is done
     * (SpiWriteAsync is non-blocking).  Wait for the DMA-done semaphore
     * before telling LVGL the flush is complete.
     *
     * If DMA is NOT configured, SpiWrite is blocking – the transfer is
     * already done when DrawBitmap returns, so the semaphore will already
     * have been given by OnTxComplete (called synchronously in that path).
     */
    xSemaphoreTake(flushSem_, portMAX_DELAY);

    lv_display_flush_ready(disp);
}

#endif /* DISPLAY_MANAGER_USE_LVGL */
