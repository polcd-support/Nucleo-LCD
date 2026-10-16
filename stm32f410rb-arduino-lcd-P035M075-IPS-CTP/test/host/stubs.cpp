#include <cstring>

#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "sim.h"

HostSerial Serial;
HostSpi SPI;
HostWire Wire;

static uint32_t g_millis = 0;

uint32_t millis(void)
{
    return g_millis;
}

void delay(uint32_t ms)
{
    g_millis += ms;
}

void delayMicroseconds(uint32_t) {}

void pinMode(uint8_t pin, uint8_t mode)
{
    Sim_PinMode(pin, mode);
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    Sim_PinWrite(pin, value);
}

void analogWrite(uint8_t pin, int value)
{
    Sim_AnalogWrite(pin, value);
    Sim_PinWrite(pin, value > 0 ? 1 : 0);
}

uint8_t HostSpi::transfer(uint8_t value)
{
    Sim_SpiByte(value);
    return 0xFF;
}

void HostSpi::transfer(void *buffer, size_t count)
{
    uint8_t *bytes = (uint8_t *)buffer;
    for (size_t i = 0; i < count; i++) Sim_SpiByte(bytes[i]);
    std::memset(buffer, 0xFF, count); /* 与 Renesas 核心一致：缓冲区被接收数据覆盖 */
}

/* ---------------------------------------------------------------- I2C */

namespace {
uint8_t g_regs[128][256];
bool g_present[128] = {false};
}

void HostWire::begin(void)
{
    /* 真实总线不会清空从机器件；这里也只复位本次传输状态。 */
    tx_length = 0;
    rx_length = 0;
    rx_index = 0;
}

void HostWire::deviceClear(void)
{
    std::memset(g_regs, 0, sizeof(g_regs));
    std::memset(g_present, 0, sizeof(g_present));
}

void HostWire::deviceSet(uint8_t address, uint8_t reg, uint8_t value)
{
    g_present[address & 0x7F] = true;
    g_regs[address & 0x7F][reg] = value;
}

uint8_t HostWire::deviceGet(uint8_t address, uint8_t reg) const
{
    return g_regs[address & 0x7F][reg];
}

void HostWire::deviceSetTouch(uint8_t address, uint8_t count, uint16_t x, uint16_t y)
{
    deviceSet(address, 0x02, count);
    deviceSet(address, 0x03, (uint8_t)((x >> 8) & 0x0F));
    deviceSet(address, 0x04, (uint8_t)(x & 0xFF));
    deviceSet(address, 0x05, (uint8_t)((y >> 8) & 0x0F));
    deviceSet(address, 0x06, (uint8_t)(y & 0xFF));
}

void HostWire::beginTransmission(uint8_t address)
{
    tx_address = address;
    tx_length = 0;
}

size_t HostWire::write(uint8_t value)
{
    if (tx_length < sizeof(tx_buffer)) tx_buffer[tx_length++] = value;
    return 1;
}

uint8_t HostWire::endTransmission(bool)
{
    if (!g_present[tx_address & 0x7F]) return 2;
    if (tx_length == 0) return 0;
    /* 第一个字节是寄存器地址，其余是要写的值（本工程只写 1 个）。 */
    const uint8_t reg = tx_buffer[0];
    for (uint8_t i = 1; i < tx_length; i++) g_regs[tx_address & 0x7F][(uint8_t)(reg + i - 1)] = tx_buffer[i];
    rx_buffer[0] = reg; /* 记下本次读起点 */
    rx_index = 0;
    rx_length = 0;
    return 0;
}

uint8_t HostWire::requestFrom(uint8_t address, uint8_t quantity)
{
    if (!g_present[address & 0x7F]) return 0;
    if (quantity > sizeof(rx_buffer)) quantity = sizeof(rx_buffer);
    const uint8_t reg = rx_buffer[0];
    for (uint8_t i = 0; i < quantity; i++) rx_buffer[i] = g_regs[address & 0x7F][(uint8_t)(reg + i)];
    rx_length = quantity;
    rx_index = 0;
    return quantity;
}

int HostWire::read(void)
{
    if (rx_index >= rx_length) return -1;
    return rx_buffer[rx_index++];
}

int HostWire::available(void)
{
    return rx_length - rx_index;
}
