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
        const void *data;      /*<! Buffer that holds the command specific data */
        size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
        unsigned int delay_ms; /*<! Delay in milliseconds after this command */
    };

    enum class ColorFormat
    {
        RGB565 = 0,
        RGB666 = 1,
    };

    /** @brief RGB element order. */
    enum class RGBElementOrder : uint8_t
    {
        RGB = 0, /*!< RGB element order: RGB */
        BGR = 1, /*!< RGB element order: BGR */
    };

    /** @brief RGB data endian. */
    enum class RGBDataEndian
    {
        BIG = 0,    /*!< RGB data endian: MSB first */
        LITTLE = 1, /*!< RGB data endian: LSB first */
    };

    enum class BitsPerPixel : uint8_t
    {
        BPP16 = 16, /*!< RGB565 */
        BPP18 = 18, /*!< RGB666 */
    };

    struct Config
    {
        int resetGpioNum = -1;      /*!< GPIO used to reset the LCD panel, set to -1 if it's not used */
        ILI9341::RGBElementOrder    rgbOrder     = ILI9341::RGBElementOrder::RGB;
        ILI9341::RGBDataEndian      dataEndian   = ILI9341::RGBDataEndian::BIG;
        ILI9341::BitsPerPixel       bitsPerPixel = ILI9341::BitsPerPixel::BPP16;
        const ILI9341::InitCommand  *initCmds    = nullptr;
        uint16_t                    initCmdsSize = 0;
        struct {
            uint32_t resetActiveHigh : 1; /*!< Setting this if the panel reset is high level active */
        } flags = {0};                    /*!< LCD panel config flags */
    };

    struct SpiIoConfig
    {
        int dcGpio = -1;
        size_t maxChunkBytes = 0; /* 0 = ask Hal::GetMaxTransferSize() */
        struct
        {
            uint32_t dcHighOnCmd : 1;
            uint32_t dcLowOnData : 1;
            uint32_t dcLowOnParam : 1;
        } flags = {};
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
        /**
         * @brief Register LCD panel IO callbacks.
         * @param[in] cbs structure with all LCD panel IO callbacks.
         * @param[in] user_ctx User private data, passed directly to callback's user_ctx.
         * @return
         *          - ILI9341_ERROR_INVALID_ARG   if parameter is invalid.
         *          - ILI9341_OK                on success.
         */
    };

    class Hal
    {
    public:
        virtual ~Hal() = default;

        // Block the calling task/thread for at least `ms` milliseconds.
        virtual void DelayMs(uint32_t ms) = 0;

        virtual void SetGpioLevel(int gpio, bool level) = 0;

        virtual void ReleasePin(int gpio) = 0;

        virtual void ConfigureOutputPin(int gpio) = 0;

        virtual ILI9341::Status SpiWrite(const uint8_t *data, size_t len) = 0;

        virtual ILI9341::Status SpiWriteAsync(const uint8_t *data, size_t len) {
            return SpiWrite(data, len);
        }

        virtual void SpiWaitIdle() {
            
        }

        // Max bytes the platform can transfer in a single SPI transaction
        // (e.g. limited by DMA descriptor size). Used to chunk large color
        // buffers in SpiIo::TxColor.
        virtual size_t GetMaxTransferSize() = 0;
    };

}

#endif /* ILI9341_TYPES_HPP_ */