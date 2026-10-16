#include "panel_p0147b0108.h"

#include "../screen_config.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P0147B0108

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 的 LCD_Init() 完全一致，
 * 逐条转写 LCD_WR_REG/LCD_WR_DATA8，delay_ms(n) 进入该行的 delay_ms 字段。 */
static const ScreenPanelReg p0147b0108_init[] = {
    {0x11, 0, 120, {}},                      /* Sleep Out */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}},  /* Porch Setting */
    {0x35, 1, 0, {0x00}},                    /* Tearing Effect Line On */
    {0x36, 1, 0, {0x00}},                    /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                    /* 16-bit/pixel（RGB565） */
    {0xB7, 1, 0, {0x35}},                    /* Entry Mode Set */
    {0xBB, 1, 0, {0x24}},                    /* VCOM Setting */
    {0xC0, 1, 0, {0x2C}},                    /* LCM Control */
    {0xC2, 1, 0, {0x01}},                    /* VDV and VRH Command Enable */
    {0xC3, 1, 0, {0x13}},                    /* VRH Set */
    {0xC4, 1, 0, {0x20}},                    /* VDV Set */
    {0xC6, 1, 0, {0x0F}},                    /* Frame Rate Control */
    {0xD0, 2, 0, {0xA7, 0xA1}},              /* Power Control 1 */
    {0xD0, 2, 0, {0xA4, 0xA1}},              /* Power Control 1（参考工程连写两次） */
    {0xD6, 1, 0, {0xA1}},                    /* Gate Control */
    {0xE0, 14, 0, {0xF0, 0x09, 0x0F, 0x0B, 0x0C, 0x18, 0x34, 0x44,
                   0x4C, 0x38, 0x12, 0x12, 0x2E, 0x35}},  /* 正极性伽马 */
    {0xE1, 14, 0, {0xF0, 0x06, 0x0C, 0x08, 0x07, 0x03, 0x34, 0x43,
                   0x4B, 0x3A, 0x15, 0x15, 0x30, 0x37}},  /* 负极性伽马 */
    {0x21, 0, 0, {}},                        /* Display Inversion On */
    {0x29, 0, 20, {}},                       /* Display On */
};

const ScreenPanel Panel_P0147B0108 = {
    "P0147B0108 (ST7789P3 172x320)",
    172,
    320,
    p0147b0108_init,
    sizeof(p0147b0108_init) / sizeof(p0147b0108_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180 */
    {34, 34, 0, 0},             /* offset_x：竖屏居中 172（参考 lcd_init.h） */
    {0, 0, 34, 34},             /* offset_y：横屏居中 172 */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* 面板侧上限：参考工程 SPI1 = PCLK/2（约 32 MHz），这里与板卡统一取 24 MHz */
    0,                          /* 参考工程 SPI1 = CPOL Low / CPHA 1Edge → mode 0 */
};

const ScreenDevice Screen_Device_P0147B0108 = {
    "P0147B0108",
    &Panel_P0147B0108,
    nullptr,                    /* 本模组无触摸（参考工程 HARDWARE/CST816 为空，demo 不碰触摸） */
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P0147B0108 */
