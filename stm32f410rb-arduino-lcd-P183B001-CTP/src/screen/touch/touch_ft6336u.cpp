#include "touch_ft6336u.h"

#include <Arduino.h>

#if SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_FT6336U

#include "../screen.h"

#define FT6_ADDR 0x38

#define FT6_REG_TD_STATUS 0x02
#define FT6_REG_TOUCH1_XH 0x03
#define FT6_REG_THRESHOLD 0x80
#define FT6_REG_PERIOD_ACTIVE 0x88
#define FT6_REG_CHIP_ID 0xA3
#define FT6_REG_G_MODE 0xA4
#define FT6_REG_POWER_MODE 0xA5

static uint8_t ft6_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    SCREEN_I2C_BUS.beginTransmission(FT6_ADDR);
    SCREEN_I2C_BUS.write(reg);
    if (SCREEN_I2C_BUS.endTransmission(false) != 0) return 0;
    if (SCREEN_I2C_BUS.requestFrom((uint8_t)FT6_ADDR, len) != len) return 0;
    for (uint8_t i = 0; i < len; i++) buf[i] = (uint8_t)SCREEN_I2C_BUS.read();
    return 1;
}

static uint8_t ft6_write_reg(uint8_t reg, uint8_t value)
{
    SCREEN_I2C_BUS.beginTransmission(FT6_ADDR);
    SCREEN_I2C_BUS.write(reg);
    SCREEN_I2C_BUS.write(value);
    return SCREEN_I2C_BUS.endTransmission() == 0;
}

uint8_t Touch_FT6336U_ChipId(void)
{
    uint8_t id = 0;
    if (!ft6_read_regs(FT6_REG_CHIP_ID, &id, 1)) return 0;
    return id;
}

void Touch_FT6336U_Init(void)
{
#if SCREEN_PIN_TOUCH_RST >= 0
    pinMode(SCREEN_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(SCREEN_PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(SCREEN_PIN_TOUCH_RST, HIGH);
    delay(100);
#endif

    /* 与参考工程 FT6236_Init() 相同的三个参数。 */
    ft6_write_reg(FT6_REG_THRESHOLD, 128);      /* 触摸阈值 */
    ft6_write_reg(FT6_REG_G_MODE, 1);           /* 中断触发模式 */
    ft6_write_reg(FT6_REG_PERIOD_ACTIVE, 10);   /* 活动模式 10 ms 上报 */
    ft6_write_reg(FT6_REG_POWER_MODE, 0);       /* 活动模式 */

#if SCREEN_DEBUG
    Serial.print(F("  chip=0x"));
    Serial.println(Touch_FT6336U_ChipId(), HEX);
#endif
}

uint8_t Touch_FT6336U_Read(uint16_t *x, uint16_t *y)
{
    uint8_t data[5];
    if (!ft6_read_regs(FT6_REG_TD_STATUS, data, sizeof(data))) return 0;

    uint8_t count = data[0] & 0x0F;
    if (count == 0) return 0;

    *x = (uint16_t)(((data[1] & 0x0F) << 8) | data[2]);
    *y = (uint16_t)(((data[3] & 0x0F) << 8) | data[4]);
    return 1;
}

const ScreenTouchDriver Touch_Driver_FT6336U = {
    "FT6336U",
    FT6_ADDR,
    Touch_FT6336U_Init,
    Touch_FT6336U_Read,
};

#endif /* SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_FT6336U */
