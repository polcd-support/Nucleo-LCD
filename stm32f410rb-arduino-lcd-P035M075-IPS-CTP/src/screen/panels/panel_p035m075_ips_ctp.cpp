#include "panel_p035m075_ips_ctp.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P035M075_IPS_CTP

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 完全一致。 */
static const ScreenPanelReg p035m075_init[] = {
    /* 正极性伽马 */
    {0xE0, 15, 0, {0x00, 0x07, 0x0F, 0x0D, 0x1B, 0x0A, 0x3C, 0x78,
                    0x4A, 0x07, 0x0E, 0x09, 0x1B, 0x1E, 0x0F}},
    /* 负极性伽马 */
    {0xE1, 15, 0, {0x00, 0x22, 0x24, 0x06, 0x12, 0x07, 0x36, 0x47,
                    0x47, 0x06, 0x0A, 0x07, 0x30, 0x37, 0x0F}},
    {0xC0, 2, 0, {0x10, 0x10}},              /* Power Control 1 */
    {0xC1, 1, 0, {0x41}},                    /* Power Control 2 */
    {0xC5, 3, 0, {0x00, 0x22, 0x80}},        /* VCOM Control */
    {0x36, 1, 0, {0x48}},                    /* MADCTL：竖屏 + BGR，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x66}},                    /* 18-bit/pixel，SPI 下唯一可用格式 */
    {0xB0, 1, 0, {0x00}},                    /* Interface Mode Control */
    {0xB1, 2, 0, {0xB0, 0x11}},              /* Frame Rate Control */
    {0xB4, 1, 0, {0x02}},                    /* Display Inversion Control */
    {0xB6, 2, 0, {0x02, 0x02}},              /* Display Function Control */
    {0xB7, 1, 0, {0xC6}},                    /* Entry Mode Set */
    {0xE9, 1, 0, {0x00}},                    /* Set Image Function */
    {0xF7, 4, 0, {0xA9, 0x51, 0x2C, 0x82}},  /* Adjust Control 3 */
    {0x11, 0, 120, {}},                      /* Sleep Out */
    {0x21, 0, 0, {}},                        /* Display Inversion On（与参考工程一致） */
    {0x29, 0, 0, {}},                        /* Display On */
};

const ScreenPanel Panel_P035M075_IPS_CTP = {
    "P035M075-IPS-CTP (ILI9488 320x480)",
    320,
    480,
    p035m075_init,
    sizeof(p035m075_init) / sizeof(p035m075_init[0]),
    {0x48, 0x88, 0x28, 0xE8},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180 */
    {0, 0, 0, 0},               /* offset_x：GRAM 320x480，无需偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB666,
    24000000UL,                 /* UNO R4 实物测试高速档；异常时回退 12 MHz */
    3,                          /* 参考工程 SPI 为 CPOL=1/CPHA=1 */
};

#if SCREEN_TOUCH_DRIVER == SCREEN_TOUCH_DRIVER_CST816
/* 参考工程 CST816 分支对这块触摸加了 15 像素 Y 偏移。 */
static const ScreenTouchConfig p035m075_touch = {
    &Touch_Driver_CST816, 320, 480, 0, 0, 0, 0, 15,
};
#else
/* FT6336U：参考工程 FT6236 分支不做任何翻转和偏移。 */
static const ScreenTouchConfig p035m075_touch = {
    &Touch_Driver_FT6336U, 320, 480, 0, 0, 0, 0, 0,
};
#endif

const ScreenDevice Screen_Device_P035M075_IPS_CTP = {
    "P035M075-IPS-CTP",
    &Panel_P035M075_IPS_CTP,
    &p035m075_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P035M075_IPS_CTP */
