/* SPI 桩件：把每个字节和当前 DC 电平转给虚拟面板。 */
#ifndef HOST_SPI_H
#define HOST_SPI_H

#include "Arduino.h"

class HostSpiSettings {
public:
    HostSpiSettings() : clock(4000000), bit_order(MSBFIRST), data_mode(SPI_MODE0) {}
    HostSpiSettings(uint32_t clock_hz, uint8_t order, uint8_t mode)
        : clock(clock_hz), bit_order(order), data_mode(mode) {}
    bool operator==(const HostSpiSettings &other) const
    {
        return clock == other.clock && bit_order == other.bit_order && data_mode == other.data_mode;
    }
    uint32_t clock;
    uint8_t bit_order;
    uint8_t data_mode;
};

typedef HostSpiSettings SPISettings;

class HostSpi {
public:
    void begin(void) {}
    void end(void) {}
    void beginTransaction(const HostSpiSettings &settings) { current = settings; }
    void endTransaction(void) {}
    uint8_t transfer(uint8_t value);
    /* 真实核心里的 transfer(void*, count) 会把收到的数据写回缓冲区，这里照做。 */
    void transfer(void *buffer, size_t count);
    const HostSpiSettings &settings(void) const { return current; }

private:
    HostSpiSettings current;
};

extern HostSpi SPI;

#endif
