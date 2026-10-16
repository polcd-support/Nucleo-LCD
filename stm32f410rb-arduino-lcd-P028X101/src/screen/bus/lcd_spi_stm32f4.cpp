#include "lcd_spi_stm32f4.h"

#include "main.h"

/* 单次 DMA 传输的长度上限（HAL_SPI_Transmit_DMA 的 Size 是 uint16_t）。 */
#define LCD_SPI_DMA_CHUNK 60000UL
/* 硬件异常保护：10 MHz 下 5760 字节约 4.6 ms，1 s 足够宽松。 */
#define LCD_SPI_TIMEOUT_MS 1000U

namespace {

bool s_open = false;
uint32_t s_hz = 0;         /* 请求值 */
uint32_t s_actual_hz = 0;  /* 分频后的真实值 */
uint8_t s_mode = 0xFF;     /* 0xFF = 尚未配置 */
int32_t s_last_error = 0;

const uint32_t kPrescalerCode[8] = {
    SPI_BAUDRATEPRESCALER_2, SPI_BAUDRATEPRESCALER_4, SPI_BAUDRATEPRESCALER_8,
    SPI_BAUDRATEPRESCALER_16, SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64,
    SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256,
};

/* 取不超过请求值的最快分频；请求值比总线/256 还慢时只能用最慢档。 */
uint32_t select_prescaler(uint32_t hz, uint32_t bus_hz)
{
    for (uint8_t index = 0; index < 8; index++) {
        const uint32_t divider = 2UL << index;
        const uint32_t actual = bus_hz / divider;
        if (actual <= hz) {
            s_actual_hz = actual;
            return kPrescalerCode[index];
        }
    }
    s_actual_hz = bus_hz / 256UL;
    return SPI_BAUDRATEPRESCALER_256;
}

bool wait_ready(void)
{
    const uint32_t start = HAL_GetTick();

    while (hLcdSpi.State != HAL_SPI_STATE_READY) {
        if ((uint32_t)(HAL_GetTick() - start) > LCD_SPI_TIMEOUT_MS) {
            s_last_error = -2;
            return false;
        }
    }
    return true;
}

}  // namespace

bool LcdSpi_Begin(uint32_t hz, uint8_t mode)
{
    if (s_open) {
        return LcdSpi_Configure(hz, mode);
    }

    hLcdSpi.Instance = SPI1;
    hLcdSpi.Init.Mode = SPI_MODE_MASTER;
    hLcdSpi.Init.Direction = SPI_DIRECTION_1LINE; /* 只发：MOSI(PA7) 单向 */
    hLcdSpi.Init.DataSize = SPI_DATASIZE_8BIT;
    hLcdSpi.Init.CLKPolarity = (mode & 0x02) ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
    hLcdSpi.Init.CLKPhase = (mode & 0x01) ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;
    hLcdSpi.Init.NSS = SPI_NSS_SOFT;
    hLcdSpi.Init.BaudRatePrescaler = select_prescaler(hz, HAL_RCC_GetPCLK2Freq());
    hLcdSpi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hLcdSpi.Init.TIMode = SPI_TIMODE_DISABLE;
    hLcdSpi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hLcdSpi.Init.CRCPolynomial = 7;

    if (HAL_SPI_Init(&hLcdSpi) != HAL_OK) {
        s_last_error = -1;
        return false;
    }

    s_hz = hz;
    s_mode = (uint8_t)(mode & 0x03);
    s_last_error = 0;
    s_open = true;
    return true;
}

bool LcdSpi_Configure(uint32_t hz, uint8_t mode)
{
    const uint8_t wanted_mode = (uint8_t)(mode & 0x03);

    if (!s_open) {
        return LcdSpi_Begin(hz, wanted_mode);
    }
    if (s_hz == hz && s_mode == wanted_mode) {
        return true;
    }

    hLcdSpi.Init.CLKPolarity = (wanted_mode & 0x02) ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
    hLcdSpi.Init.CLKPhase = (wanted_mode & 0x01) ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;
    hLcdSpi.Init.BaudRatePrescaler = select_prescaler(hz, HAL_RCC_GetPCLK2Freq());

    if (HAL_SPI_Init(&hLcdSpi) != HAL_OK) { /* 状态已是 READY，HAL 只重写 CR1/CR2 */
        s_last_error = -1;
        return false;
    }

    s_hz = hz;
    s_mode = wanted_mode;
    s_last_error = 0;
    return true;
}

bool LcdSpi_WriteDma(const uint8_t *data, uint32_t length)
{
    if (!s_open || data == nullptr) {
        s_last_error = -3;
        return false;
    }

    while (length > 0) {
        const uint32_t take = (length > LCD_SPI_DMA_CHUNK) ? LCD_SPI_DMA_CHUNK : length;

        if (HAL_SPI_Transmit_DMA(&hLcdSpi, data, (uint16_t)take) != HAL_OK) {
            s_last_error = -4;
            return false;
        }
        if (!wait_ready()) {
            return false;
        }

        data += take;
        length -= take;
    }

    s_last_error = 0;
    return true;
}

bool LcdSpi_WriteBlocking(const uint8_t *data, uint32_t length)
{
    if (!s_open || data == nullptr) {
        s_last_error = -3;
        return false;
    }

    while (length > 0) {
        const uint32_t take = (length > 60000UL) ? 60000UL : length;

        if (HAL_SPI_Transmit(&hLcdSpi, data, (uint16_t)take, LCD_SPI_TIMEOUT_MS) != HAL_OK) {
            s_last_error = -5;
            return false;
        }

        data += take;
        length -= take;
    }

    s_last_error = 0;
    return true;
}

int32_t LcdSpi_LastError(void)
{
    return s_last_error;
}

uint32_t LcdSpi_ActualHz(void)
{
    return s_actual_hz;
}
