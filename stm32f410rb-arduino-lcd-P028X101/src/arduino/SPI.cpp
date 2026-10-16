#include "SPI.h"

#include <string.h>

#include "../screen/bus/lcd_spi_stm32f4.h"
#include "Arduino.h"

/* Arduino 的 SPI.begin() 不带参数，默认 4 MHz；真正的时钟/模式由 beginTransaction() 设定。 */
#define SPI_DEFAULT_HZ 4000000UL

SPIClass SPI;

SPISettings::SPISettings() : clock(SPI_DEFAULT_HZ), bit_order(MSBFIRST), data_mode(SPI_MODE0) {}

SPISettings::SPISettings(uint32_t clock_hz, uint8_t order, uint8_t mode)
    : clock(clock_hz), bit_order(order), data_mode(mode)
{
}

void SPIClass::begin(void)
{
    LcdSpi_Begin(SPI_DEFAULT_HZ, SPI_MODE0);
}

void SPIClass::end(void)
{
    /* 总线由 LCD 后端持有，运行时不做反初始化。 */
}

void SPIClass::beginTransaction(const SPISettings &settings)
{
    LcdSpi_Configure(settings.clock, settings.data_mode);
}

void SPIClass::endTransaction(void)
{
}

uint8_t SPIClass::transfer(uint8_t value)
{
    LcdSpi_WriteBlocking(&value, 1);
    return 0xFF; /* 只发不收回，无有效接收数据 */
}

void SPIClass::transfer(void *buffer, size_t count)
{
    if (buffer == nullptr || count == 0) {
        return;
    }
    LcdSpi_WriteBlocking((const uint8_t *)buffer, (uint32_t)count);
    memset(buffer, 0xFF, count);
}
