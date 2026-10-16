#include "touch_cst816.h"

#include <Arduino.h>

#if SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816

#include "../screen.h"

#define CST816_ADDR 0x15

#define CST816_REG_GESTURE_ID 0x01
#define CST816_REG_FINGER_NUM 0x02
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

    /* 本驱动只做 I2C 轮询，不使用 INT（转接板 D4 不参与控制）。四个参考工程都不写 0xFA，
     * 这里显式写 0：关闭片内触摸/变化中断输出——主机不处理 INT 时，未清除的中断事件
     * 会把芯片锁在事件态，表现成触摸完全没反应。 */
    cst_write_reg(CST816_REG_IRQ_CTL, 0x00);
    cst_write_reg(CST816_REG_DIS_AUTO_SLEEP, 0x01);/* 演示期间不自动休眠 */

#if SCREEN_DEBUG
    Serial.print(F("  chip=0x"));
    Serial.println(Touch_CST816_ChipId(), HEX);
#endif
}

uint8_t Touch_CST816_Read(uint16_t *x, uint16_t *y)
{
    /* 从 0x01 起一次读完手势/触点数/坐标：读完即清掉片内的触摸事件状态。 */
    uint8_t data[6];
    if (!cst_read_regs(CST816_REG_GESTURE_ID, data, sizeof(data))) return 0;
    if (data[1] == 0xFF || (data[1] & 0x0F) == 0) return 0; /* 0xFF = 睡眠态，不是触摸 */

    *x = (uint16_t)(((data[2] & 0x0F) << 8) | data[3]);
    *y = (uint16_t)(((data[4] & 0x0F) << 8) | data[5]);
    return 1;
}

const ScreenTouchDriver Touch_Driver_CST816 = {
    "CST816",
    CST816_ADDR,
    Touch_CST816_Init,
    Touch_CST816_Read,
};

#endif /* SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816 */
