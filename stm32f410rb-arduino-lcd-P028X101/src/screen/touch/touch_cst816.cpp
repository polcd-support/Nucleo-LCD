#include "touch_cst816.h"

#include <Arduino.h>

#if SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816

#include "../screen.h"

#define CST816_ADDR 0x15

#define CST816_REG_FINGER_NUM 0x02
#define CST816_REG_XPOS_H 0x03
#define CST816_REG_CHIP_ID 0xA7
#define CST816_REG_IRQ_CTL 0xFA
#define CST816_REG_DIS_AUTO_SLEEP 0xFE

static uint8_t cst_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    SCREEN_I2C_BUS.beginTransmission(CST816_ADDR);
    SCREEN_I2C_BUS.write(reg);
    if (SCREEN_I2C_BUS.endTransmission(false) != 0) return 0;
    if (SCREEN_I2C_BUS.requestFrom((uint8_t)CST816_ADDR, len) != len) return 0;
    for (uint8_t i = 0; i < len; i++) buf[i] = (uint8_t)SCREEN_I2C_BUS.read();
    return 1;
}

static uint8_t cst_write_reg(uint8_t reg, uint8_t value)
{
    SCREEN_I2C_BUS.beginTransmission(CST816_ADDR);
    SCREEN_I2C_BUS.write(reg);
    SCREEN_I2C_BUS.write(value);
    return SCREEN_I2C_BUS.endTransmission() == 0;
}

uint8_t Touch_CST816_ChipId(void)
{
    uint8_t id = 0;
    if (!cst_read_regs(CST816_REG_CHIP_ID, &id, 1)) return 0;
    return id;
}

void Touch_CST816_Init(void)
{
#if SCREEN_PIN_TOUCH_RST >= 0
    pinMode(SCREEN_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(SCREEN_PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(SCREEN_PIN_TOUCH_RST, HIGH);
    delay(100);
#endif

    cst_write_reg(CST816_REG_IRQ_CTL, 0x61);       /* 使能触摸中断并置 1，读取坐标后自动清 */
    cst_write_reg(CST816_REG_DIS_AUTO_SLEEP, 0x01);/* 演示期间不自动休眠 */

#if SCREEN_DEBUG
    Serial.print(F("  chip=0x"));
    Serial.println(Touch_CST816_ChipId(), HEX);
#endif
}

uint8_t Touch_CST816_Read(uint16_t *x, uint16_t *y)
{
    uint8_t data[5];
    if (!cst_read_regs(CST816_REG_FINGER_NUM, data, sizeof(data))) return 0;
    if ((data[0] & 0x0F) == 0) return 0;

    *x = (uint16_t)(((data[1] & 0x0F) << 8) | data[2]);
    *y = (uint16_t)(((data[3] & 0x0F) << 8) | data[4]);
    return 1;
}

const ScreenTouchDriver Touch_Driver_CST816 = {
    "CST816",
    CST816_ADDR,
    Touch_CST816_Init,
    Touch_CST816_Read,
};

#endif /* SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816 */
