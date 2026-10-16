#include "Wire.h"

#include "main.h"

/* 触摸每 10 ms 只读 5 字节，100 kHz 足够；转接板 U2 的 10 k 上拉下 400 kHz 上升沿偏慢。 */
#define WIRE_DEFAULT_HZ 100000UL
#define WIRE_TIMEOUT_MS 100U

TwoWire Wire;

void TwoWire::begin(void)
{
    if (started) {
        return;
    }

    hLcdI2c.Instance = I2C1;
    hLcdI2c.Init.ClockSpeed = clock_hz;
    hLcdI2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hLcdI2c.Init.OwnAddress1 = 0;
    hLcdI2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hLcdI2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hLcdI2c.Init.OwnAddress2 = 0;
    hLcdI2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hLcdI2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hLcdI2c) != HAL_OK) {
        Error_Handler();
    }

    started = true;
}

void TwoWire::setClock(uint32_t hz)
{
    clock_hz = hz;
    if (started) {
        hLcdI2c.Init.ClockSpeed = hz;
        if (HAL_I2C_Init(&hLcdI2c) != HAL_OK) {
            Error_Handler();
        }
    }
}

void TwoWire::beginTransmission(uint8_t address)
{
    tx_address = address;
    tx_length = 0;
}

size_t TwoWire::write(uint8_t value)
{
    if (tx_length < sizeof(tx_buffer)) {
        tx_buffer[tx_length++] = value;
        return 1;
    }
    return 0;
}

uint8_t TwoWire::endTransmission(bool stop)
{
    (void)stop; /* 主机端总是发 STOP；后接的读操作由 requestFrom 重新起 START。 */

    if (tx_length == 0) {
        return 0;
    }

    if (tx_length == 1) {
        /* 只有寄存器地址：留着给后面的 requestFrom 做寄存器读。 */
        pending_register = tx_buffer[0];
        pending_register_valid = true;
        return 0;
    }

    /* 写寄存器：reg + value（本工程只写 1 字节数据）。 */
    if (HAL_I2C_Mem_Write(&hLcdI2c, (uint16_t)(tx_address << 1), tx_buffer[0],
                          I2C_MEMADD_SIZE_8BIT, &tx_buffer[1], (uint16_t)(tx_length - 1),
                          WIRE_TIMEOUT_MS) != HAL_OK) {
        return 2;
    }

    tx_length = 0;
    return 0;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
    if (quantity > sizeof(rx_buffer)) {
        quantity = (uint8_t)sizeof(rx_buffer);
    }

    rx_length = 0;
    rx_index = 0;

    if (pending_register_valid) {
        if (HAL_I2C_Mem_Read(&hLcdI2c, (uint16_t)(address << 1), pending_register,
                             I2C_MEMADD_SIZE_8BIT, rx_buffer, quantity,
                             WIRE_TIMEOUT_MS) != HAL_OK) {
            return 0;
        }
        pending_register_valid = false;
    } else if (HAL_I2C_Master_Receive(&hLcdI2c, (uint16_t)(address << 1), rx_buffer, quantity,
                                      WIRE_TIMEOUT_MS) != HAL_OK) {
        return 0;
    }

    rx_length = quantity;
    return quantity;
}

int TwoWire::read(void)
{
    if (rx_index >= rx_length) {
        return -1;
    }
    return rx_buffer[rx_index++];
}

int TwoWire::available(void)
{
    return (int)rx_length - (int)rx_index;
}
