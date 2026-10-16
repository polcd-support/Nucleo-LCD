/*
 * Arduino SPI API 的等价物。
 *
 * 本工程只有一条 SPI 总线（SPI1，接 LCD），总线真正的主人是
 * src/screen/bus/lcd_spi_stm32f4.cpp，这里只是把它包成 Arduino 的 SPI 用法：
 * 屏幕层在 DMA 后端不可用时会退回“begin() + beginTransaction() + transfer()”的逐字节路径，
 * 那条路径走的就是这里的阻塞式发送。
 *
 * 注意：LCD 的 CS/DC 是普通 GPIO，由 screen 层直接控制，不在 SPI 外设里。
 */
#ifndef ARDUINO_COMPAT_SPI_H
#define ARDUINO_COMPAT_SPI_H

#include <stddef.h>
#include <stdint.h>

#define MSBFIRST 1
#define LSBFIRST 0

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
public:
    SPISettings();
    SPISettings(uint32_t clock_hz, uint8_t order, uint8_t mode);

    bool operator==(const SPISettings &other) const
    {
        return clock == other.clock && bit_order == other.bit_order && data_mode == other.data_mode;
    }

    uint32_t clock;
    uint8_t bit_order;
    uint8_t data_mode;
};

class SPIClass {
public:
    void begin(void);
    void end(void);
    void beginTransaction(const SPISettings &settings);
    void endTransaction(void);
    uint8_t transfer(uint8_t value);
    /* 与 Arduino 相同：发送 count 字节，并把缓冲区覆盖成接收到的数据（本总线没有回读，填 0xFF）。 */
    void transfer(void *buffer, size_t count);
};

extern SPIClass SPI;

#endif /* ARDUINO_COMPAT_SPI_H */
