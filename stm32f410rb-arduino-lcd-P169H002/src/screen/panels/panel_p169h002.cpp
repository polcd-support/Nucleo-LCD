#include "panel_p169h002.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P169H002

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 的 LCD_Init() 完全一致。
 * #if USE_HORIZONTAL 分支取 USE_HORIZONTAL==0 的值（0x36 写 0x00）。 */
static const ScreenPanelReg p169h002_init[] = {
    {0x11, 0, 120, {}},                          /* Sleep Out */
    {0x36, 1, 0, {0x00}},                        /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                        /* 16-bit/pixel = RGB565 */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}},/* Porch Setting */
    {0xB7, 1, 0, {0x35}},                        /* Gate Control */
    {0xBB, 1, 0, {0x32}},                        /* VCOM = 1.35 V */
    {0xC2, 1, 0, {0x01}},                        /* VDV/VRH Enable */
    {0xC3, 1, 0, {0x15}},                        /* VRH = 4.8 V（颜色深度） */
    {0xC4, 1, 0, {0x20}},                        /* VDV = 0 V */
    {0xC6, 1, 0, {0x0F}},                        /* 60 Hz */
    {0xD0, 2, 0, {0xA4, 0xA1}},                  /* Power Control */
    {0xE0, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}}, /* 正极性伽马 */
    {0xE1, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}}, /* 负极性伽马 */
    {0x21, 0, 0, {}},                            /* Display Inversion On（与参考工程一致） */
    {0x29, 0, 0, {}},                            /* Display On */
};

const ScreenPanel Panel_P169H002 = {
    "P169H002 (ST7789 240x280)",
    240,
    280,
    p169h002_init,
    sizeof(p169h002_init) / sizeof(p169h002_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180 */
    /* 偏移照抄参考工程 LCD_Address_Set()：方向 3 给列地址 +80，其余方向列为 0。 */
    {0, 0, 0, 80},              /* offset_x */
    /* 方向 0 给行地址 +20，方向 1 给行地址 +80，方向 2/3 行为 0。 */
    {20, 80, 0, 0},             /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* 面板侧上限；实际受板级 SCREEN_SPI_HZ 限制 */
    0,                          /* 参考工程 SPI1 为 CPOL=0/CPHA=0 */
};

/* 本模组的触摸是 CST816（参考工程 HARDWARE/CST816/CST816.c），
 * 其 TOUCH_OFFSET_Y = 5、REVERSE = 0，这里改由 ScreenTouchConfig.offset_y 表达。 */
static const ScreenTouchConfig p169h002_touch = {
    &Touch_Driver_CST816, 240, 280, 0, 0, 0, 0, 5,
};

const ScreenDevice Screen_Device_P169H002 = {
    "P169H002",
    &Panel_P169H002,
    &p169h002_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P169H002 */
