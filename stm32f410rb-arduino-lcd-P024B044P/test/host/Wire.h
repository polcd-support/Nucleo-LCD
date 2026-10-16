/* I2C 桩件：一个可写的寄存器文件，模拟 FT6336U / CST816。 */
#ifndef HOST_WIRE_H
#define HOST_WIRE_H

#include "Arduino.h"

class HostWire {
public:
    void begin(void);
    void setClock(uint32_t hz) { clock_hz = hz; }
    void beginTransmission(uint8_t address);
    size_t write(uint8_t value);
    uint8_t endTransmission(bool stop = true);
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    int read(void);
    int available(void);

    /* 测试侧的注入接口。 */
    void deviceSet(uint8_t address, uint8_t reg, uint8_t value);
    void deviceSetTouch(uint8_t address, uint8_t count, uint16_t x, uint16_t y);
    void deviceClear(void);
    uint32_t clock(void) const { return clock_hz; }

private:
    uint8_t tx_address = 0;
    uint8_t tx_buffer[16];
    uint8_t tx_length = 0;
    uint8_t rx_buffer[16];
    uint8_t rx_length = 0;
    uint8_t rx_index = 0;
    uint32_t clock_hz = 100000;
};

extern HostWire Wire;

#endif
