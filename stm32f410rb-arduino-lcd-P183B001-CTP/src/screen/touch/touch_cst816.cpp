#include "touch_cst816.h"

#include <Arduino.h>

#if SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816

#include "../screen.h"

#define CST816_ADDR 0x15

#define CST816_REG_FINGER_NUM 0x02
#define CST816_REG_CHIP_ID 0xA7
#define CST816_REG_DIS_AUTO_SLEEP 0xFE

static uint8_t s_read_error_stage = 0;
static uint8_t s_read_error_value = 0;

static uint8_t cst_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    SCREEN_I2C_BUS.beginTransmission(CST816_ADDR);
    SCREEN_I2C_BUS.write(reg);
    const uint8_t status = SCREEN_I2C_BUS.endTransmission(true);
    if (status != 0) {
        s_read_error_stage = 1;
        s_read_error_value = status;
        return 0;
    }
    const uint8_t received = SCREEN_I2C_BUS.requestFrom((uint8_t)CST816_ADDR, len);
    if (received != len) {
        s_read_error_stage = 2;
        s_read_error_value = received;
        return 0;
    }
    for (uint8_t i = 0; i < len; i++) buf[i] = (uint8_t)SCREEN_I2C_BUS.read();
    s_read_error_stage = 0;
    s_read_error_value = 0;
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
    /* UNO R4 工程在这之前还要先停掉 Wire 再复位 CST816（RA4M1 的 I2C 外设要求），
     * F410 的硬件 I2C1 没有这个问题，复位时序保持一致即可。 */
    pinMode(SCREEN_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(SCREEN_PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(SCREEN_PIN_TOUCH_RST, HIGH);
    delay(200);
#endif

    /* 演示约 16 秒后才进入手写页，轮询期间禁止自动休眠。 */
    const uint8_t sleep_ok = cst_write_reg(CST816_REG_DIS_AUTO_SLEEP, 0x01);

#if SCREEN_DEBUG
    Serial.print(F("touch init: chip=0x"));
    Serial.println(Touch_CST816_ChipId(), HEX);
    Serial.print(F("  disable_auto_sleep="));
    Serial.println(sleep_ok ? F("ok") : F("failed"));
#else
    (void)sleep_ok;
#endif
}

uint8_t Touch_CST816_Read(uint16_t *x, uint16_t *y)
{
    /* 与参考驱动一致：先单读触点数，有触点时再连读 4 字节坐标。 */
    uint8_t fingers = 0;
    if (!cst_read_regs(CST816_REG_FINGER_NUM, &fingers, 1)) {
#if SCREEN_DEBUG
        static uint32_t last_error_log = 0;
        const uint32_t now = millis();
        if (now - last_error_log >= 1000) {
            Serial.print(F("touch i2c read failed: stage="));
            Serial.print(s_read_error_stage);
            Serial.print(F(" value="));
            Serial.print(s_read_error_value);
            Serial.println();
            last_error_log = now;
        }
#endif
        return 0;
    }
    if (fingers == 0xFF || (fingers & 0x0F) == 0) return 0;

    uint8_t data[4];
    if (!cst_read_regs(0x03, data, sizeof(data))) return 0;

    *x = (uint16_t)(((data[0] & 0x0F) << 8) | data[1]);
    *y = (uint16_t)(((data[2] & 0x0F) << 8) | data[3]);
    return 1;
}

const ScreenTouchDriver Touch_Driver_CST816 = {
    "CST816",
    CST816_ADDR,
    Touch_CST816_Init,
    Touch_CST816_Read,
};

#endif /* SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816 */
