#include "panel_p020b025p_ips_v2.h"

#include "../screen_config.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P020B025P_IPS_V2

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 的 LCD_Init() 完全一致，
 * USE_HORIZONTAL 取 0。末尾的 0x2C + 20 ms 延时也是参考工程的写法（本工程每次
 * Screen_SetWindow 都会重新设窗口，不依赖这里）。 */
static const ScreenPanelReg p020b025p_init[] = {
    {0x11, 0, 120, {}},                        /* Sleep Out */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}},  /* Porch Setting */
    {0x35, 1, 0, {0x00}},                      /* Tearing Effect Line On */
    {0x36, 1, 0, {0x00}},                      /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                      /* 16-bit/pixel（RGB565） */
    {0xB7, 1, 0, {0x06}},                      /* Entry Mode Set */
    {0xBB, 1, 0, {0x20}},                      /* VCOM Setting */
    {0xC0, 1, 0, {0x2C}},                      /* LCM Control */
    {0xC2, 1, 0, {0x01}},                      /* VDV and VRH Command Enable */
    {0xC3, 1, 0, {0x0F}},                      /* VRH Set */
    {0xC6, 1, 0, {0x0F}},                      /* VCOM Offset Set */
    {0xD0, 1, 0, {0xA7}},                      /* Power Control 1 */
    {0xD0, 2, 0, {0xA4, 0xA1}},                /* Power Control 1（第 2 次，参考工程连写两次） */
    {0xD6, 1, 0, {0xA1}},                      /* Power Control 2 */
    /* 正极性伽马 */
    {0xE0, 14, 0, {0xF0, 0x06, 0x0E, 0x08, 0x09, 0x17, 0x37, 0x54,
                   0x4C, 0x39, 0x14, 0x12, 0x2F, 0x34}},
    /* 负极性伽马 */
    {0xE1, 14, 0, {0xF0, 0x0F, 0x14, 0x0C, 0x0A, 0x04, 0x37, 0x43,
                   0x4C, 0x36, 0x12, 0x14, 0x2D, 0x36}},
    {0x21, 0, 0, {}},                          /* Display Inversion On */
    {0x29, 0, 0, {}},                          /* Display On */
    {0x2C, 0, 20, {}},                         /* 参考工程在此显式写 0x2C 并延时 20 ms */
};

const ScreenPanel Panel_P020B025P_IPS_V2 = {
    "P020B025P-IPS-V2 (ST7789P3 240x320)",
    240,
    320,
    p020b025p_init,
    sizeof(p020b025p_init) / sizeof(p020b025p_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180（USE_HORIZONTAL 0..3） */
    {0, 0, 0, 0},               /* offset_x：GRAM 240x320，无需偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* ST7789 系列面板侧上限，与板卡 SCREEN_SPI_HZ 一致 */
    0,                          /* 参考工程 SPI1 为 CPOL=0/CPHA=0（LOW + 1EDGE） */
};

const ScreenDevice Screen_Device_P020B025P_IPS_V2 = {
    "P020B025P-IPS-V2",
    &Panel_P020B025P_IPS_V2,
    nullptr,                    /* 本模组无触摸 */
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P020B025P_IPS_V2 */
