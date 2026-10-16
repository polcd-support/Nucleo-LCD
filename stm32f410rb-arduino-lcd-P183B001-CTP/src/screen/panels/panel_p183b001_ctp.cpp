#include "panel_p183b001_ctp.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P183B001_CTP

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 完全一致，
 * USE_HORIZONTAL==0 分支（0x36 = 0x00）。 */
static const ScreenPanelReg p183b001_init[] = {
    {0x11, 0, 120, {}},                      /* Sleep Out，之后延时 120 ms */
    {0x36, 1, 0, {0x00}},                    /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                    /* 16-bit/pixel（RGB565） */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}}, /* Porch Setting */
    {0xB7, 1, 0, {0x35}},                    /* Gate Control */
    {0xBB, 1, 0, {0x32}},                    /* VCOM Setting（1.35 V） */
    {0xC2, 1, 0, {0x01}},                    /* VDDS Setting */
    {0xC3, 1, 0, {0x15}},                    /* VRH Setting（GVDD = 4.8 V） */
    {0xC4, 1, 0, {0x20}},                    /* VDV Setting（0x20 = 0 V） */
    {0xC6, 1, 0, {0x0F}},                    /* Frame Rate Control（60 Hz） */
    {0xD0, 2, 0, {0xA4, 0xA1}},              /* Power Control 1 */
    {0xE0, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}}, /* 正极性伽马 */
    {0xE1, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}}, /* 负极性伽马 */
    {0x21, 0, 0, {}},                        /* Display Inversion On（与参考工程一致） */
    {0x29, 0, 0, {}},                        /* Display On */
};

const ScreenPanel Panel_P183B001_CTP = {
    "P183B001-CTP (ST7789 240x284)",
    240,
    284,
    p183b001_init,
    sizeof(p183b001_init) / sizeof(p183b001_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180 */
    {0, 0, 0, 0},               /* offset_x：参考工程四个方向都无偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* 面板侧上限；板级 SCREEN_SPI_HZ 取 24 MHz 高速档 */
    0,                          /* 参考工程 SPI 为 CPOL=LOW / CPHA=1EDGE */
};

/* CST816：实物确认 Y 轴方向与显示相反；翻转后再叠加参考工程的 TOUCH_OFFSET_Y = -10。 */
static const ScreenTouchConfig p183b001_touch = {
    &Touch_Driver_CST816, 240, 284, 0, 0, 1, 0, -10,
};

const ScreenDevice Screen_Device_P183B001_CTP = {
    "P183B001-CTP",
    &Panel_P183B001_CTP,
    &p183b001_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P183B001_CTP */
