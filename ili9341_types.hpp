#ifndef ILI9341_TYPES_HPP_
#define ILI9341_TYPES_HPP_
#include <cstring>
#include <cstdint>

namespace ILI9341
{
    enum class Status
    {
        Ok                      = 0,
        ErrorNoMem              = -1,
        ErrorInvalidArg         = -2,
        ErrorInvalidColorFormat = -3,
        ErrorIo                 = -4,
        ErrorNotSupported       = -5,
    };

    /** @brief LCD panel initialization commands. */
    struct InitCommand
    {
        int cmd;               /*<! The specific LCD command */
        const void *data;      /*<! Buffer holding the command's parameter bytes. Must remain valid until Panel::Init() returns - Panel does not copy it. */
        size_t dataBytes;      /*<! Size of `data` in memory, in bytes */
        unsigned int delayMs;  /*<! Delay in milliseconds after this command */
    };

    /** @brief RGB element order. */
    enum class RgbElementOrder : uint8_t
    {
        Rgb = 0, /*!< RGB element order: RGB */
        Bgr = 1, /*!< RGB element order: BGR */
    };

    enum class BitsPerPixel : uint8_t
    {
        Bpp16 = 16, /*!< RGB565 */
        Bpp18 = 18, /*!< RGB666 */
    };

    struct GpioSpec {
        void *port = nullptr;
        int  pin = -1;
        bool IsValid() const {
            return pin >= 0;
        }
    };

    struct Config
    {
        GpioSpec resetGpio; /*!< GPIO used to reset the LCD panel, set to -1 if it's not used */
        ILI9341::RgbElementOrder rgbOrder = ILI9341::RgbElementOrder::Rgb;
        ILI9341::BitsPerPixel bitsPerPixel = ILI9341::BitsPerPixel::Bpp16;
        const ILI9341::InitCommand *initCmds = nullptr;
        uint16_t initCmdsSize = 0;
        struct
        {
            uint32_t resetActiveHigh : 1; /*!< Setting this if the panel reset is high level active */
        } flags = {0};                    /*!< LCD panel config flags */
    };

    struct SpiIoConfig
    {
        GpioSpec dcGpio;
        size_t maxChunkBytes = 0; /* 0 = ask Hal::GetMaxTransferSize() */
        struct
        {
            uint8_t dcCmdLevel   : 1;
            uint8_t dcParamLevel : 1;
            uint8_t dcDataLevel  : 1;
        } flags = {0, 1, 1};
    };

    class Io
    {
    public:
        virtual ~Io() = default;
        /**
         * @brief Transmit LCD command and receive corresponding parameters.
         * @param[in]  lcdCmd The specific LCD command, set to -1 if no command needed.
         * @param[out] param Buffer for the command data.
         * @param[in]  paramSize Size of `param` buffer.
         * @return
         *          - ILI9341_ERROR_INVALID_ARG   if parameter is invalid.
         *          - ILI9341_ERROR_NOT_SUPPORTED if read is not supported by transport.
         *          - ILI9341_OK                on success.
         */
        virtual ILI9341::Status RxParam(int lcdCmd, void *param, size_t paramSize) = 0;
        /**
         * @brief Transmit LCD command and corresponding parameters.
         * @param[in] lcdCmd The specific LCD command.
         * @param[in] param Buffer that holds the command specific parameters, set to NULL if no parameter is needed for the command.
         * @param[in] paramSize Size of `param` in memory, in bytes, set to zero if no parameter is needed for the command.
         * @return
         *          - ILI9341_ERROR_INVALID_ARG   if parameter is invalid.
         *          - ILI9341_OK                on success.
         */
        virtual ILI9341::Status TxParam(int lcdCmd, const void *param, size_t paramSize) = 0;
        /**
         * @brief Transmit LCD RGB data.
         * @param[in] lcdCmd The specific LCD command.
         * @param[in] color Buffer that holds the RGB color data.
         * @param[in] color_size Size of `color` in memory, in bytes.
         * @return
         *          - ILI9341_ERROR_INVALID_ARG   if parameter is invalid.
         *          - ILI9341_OK                on success.
         */
        virtual ILI9341::Status TxColor(int lcdCmd, const void *color, size_t color_size) = 0;
    };

    class Hal
    {
    public:
        virtual ~Hal() = default;

        using TransDoneCallback = void (*)(void *userCtx);

        /** @brief Block the calling task/thread for at least `ms` milliseconds. */
        virtual void DelayMs(uint32_t ms) = 0;
        /** @brief Drive a GPIO pin high or low. Pin must already be configured as output. */
        virtual void SetGpioLevel(const GpioSpec &gpio, bool level) = 0;
        /** @brief Release a pin previously configured by ConfigureOutputPin/Reset, returning it to its default (unused) state. */
        virtual void ReleasePin(const GpioSpec &gpio) = 0;
        /** @brief Configure a GPIO pin as push-pull output. Called once before the pin is first used. */
        virtual void ConfigureOutputPin(const GpioSpec &gpio) = 0;
        /** @brief Blocking SPI write of exactly `len` bytes from `data`. Returns once the transfer completes. */
        virtual ILI9341::Status SpiWrite(const uint8_t *data, size_t len) = 0;
        /**
         * @brief Non-blocking SPI write; queue-and-return so multiple chunks can pipeline.
         * @param isLastChunk True on the final chunk of a TxColor() transfer. Platforms that
         * implement RegisterTransDoneCallback() must fire that callback only when the
         * transaction carrying isLastChunk=true has actually completed on the bus.
         * Default implementation falls back to the blocking SpiWrite() and ignores this flag.
         */
        virtual ILI9341::Status SpiWriteAsync(const uint8_t *data, size_t len, bool isLastChunk)
        {
            (void)isLastChunk;
            return this->SpiWrite(data, len);
        }
        /**
         * @brief Block until all SpiWriteAsync() transfers queued so far have completed.
         * Must be called before changing the DC line level or starting a different
         * SPI transaction. Default (blocking SpiWrite only) has nothing to wait for.
         */
        virtual ILI9341::Status SpiWaitIdle()
        {
            return ILI9341::Status::Ok;
        }
        /**
         * @brief Register a callback fired when a SpiWriteAsync() chain has completed
         * on the bus (isLastChunk transaction done) - typically invoked from an
         * ISR/DMA-done context, so implementations must keep it minimal and
         * interrupt-safe. Optional: platforms without async DMA support can leave
         * this unimplemented; SpiIo does not depend on it directly.
         */
        virtual void RegisterTransDoneCallback(TransDoneCallback cb, void *userCtx)
        {
            (void)cb;
            (void)userCtx;
        }
        /**
         * @brief Max bytes the platform can transfer in a single SPI transaction
         * (e.g. limited by DMA descriptor size). Used to chunk large color
         * buffers in SpiIo::TxColor.
         */
        virtual size_t GetMaxTransferSize() = 0;
    };

}

#endif /* ILI9341_TYPES_HPP_ */