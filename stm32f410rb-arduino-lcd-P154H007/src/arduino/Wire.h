/*
 * Arduino Wire（I2C 主机）API 的等价物，落在 I2C1 上（PB8/PB9，即 UNO R3 排针的 SCL/SDA 位）。
 *
 * 只实现本工程用到的“寄存器读写”用法：
 *   beginTransmission(addr); write(reg); [write(value);] endTransmission();
 *   requestFrom(addr, len); read() ...
 * 传输语义与 test/host/Wire.h 的桩件一致（write 只有寄存器地址时，读操作延迟到 requestFrom）。
 */
#ifndef ARDUINO_COMPAT_WIRE_H
#define ARDUINO_COMPAT_WIRE_H

#include <stddef.h>
#include <stdint.h>

class TwoWire {
public:
    void begin(void);
    void setClock(uint32_t hz);

    void beginTransmission(uint8_t address);
    size_t write(uint8_t value);
    /* 返回 0 = 成功，2 = 从机无应答/总线错误（与 Arduino 的返回值含义一致）。 */
    uint8_t endTransmission(bool stop = true);

    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    int read(void);
    int available(void);

private:
    uint8_t rx_buffer[16];
    uint8_t rx_length = 0;
    uint8_t rx_index = 0;
    uint8_t tx_address = 0;
    uint8_t tx_buffer[4];
    uint8_t tx_length = 0;
    uint8_t pending_register = 0;
    bool pending_register_valid = false;
    uint32_t clock_hz = 100000;
    bool started = false;
};

extern TwoWire Wire;

#endif /* ARDUINO_COMPAT_WIRE_H */
