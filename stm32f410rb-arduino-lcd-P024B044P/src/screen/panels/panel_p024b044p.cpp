#include "panel_p024b044p.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P024B044P

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 完全一致。
 * 参考工程末尾的 read_lcd_id() 分支需要 MISO 回读（转接板没有回读线），
 * 这里直接取 else 分支的 0x21（Display Inversion On）。 */
static const ScreenPanelReg p024b044p_init[] = {
    {0x11, 0, 120, {}},                            /* Sleep Out */
    {0x36, 1, 0, {0x00}},                          /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                          /* 16-bit/pixel（RGB565） */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}},  /* Porch Setting */
    {0xB7, 1, 0, {0x35}},                          /* Gate Control */
    {0xBB, 1, 0, {0x32}},                          /* VCOM Setting（Vcom=1.35V） */
    {0xC2, 1, 0, {0x01}},                          /* VDV/VRH Enable */
    {0xC3, 1, 0, {0x15}},                          /* VRH Set（GVDD=4.8V） */
    {0xC4, 1, 0, {0x20}},                          /* VDV Set（0x20:0V） */
    {0xC6, 1, 0, {0x0F}},                          /* Frame Rate Control（0x0F:60Hz） */
    {0xD0, 2, 0, {0xA4, 0xA1}},                    /* Power Control */
    /* 正极性伽马 */
    {0xE0, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},
    /* 负极性伽马 */
    {0xE1, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},
    {0x21, 0, 0, {}},                              /* Display Inversion On（参考工程 else 分支） */
    {0x29, 0, 0, {}},                              /* Display On */
};

const ScreenPanel Panel_P024B044P = {
    "P024B044P (ST7789 240x320)",
    240,
    320,
    p024b044p_init,
    sizeof(p024b044p_init) / sizeof(p024b044p_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180（ST7789 标准值） */
    {0, 0, 0, 0},               /* offset_x：GRAM 240x320，无需偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* 面板侧上限：ST7789 4 线 SPI 超过 24 MHz */
    0,                          /* 参考工程 SPI 为 CPOL=0/CPHA=0（Mode 0） */
};

/* FT6236（FT6xxx 寄存器，兼容 FT6336U）：参考工程 TOUCH_OFFSET_X/Y=0、REVERSE_X/Y=0，
 * 原生坐标 240x320 与显示坐标一致。 */
static const ScreenTouchConfig p024b044p_touch = {
    &Touch_Driver_FT6336U, 240, 320, 0, 0, 0, 0, 0,
};

const ScreenDevice Screen_Device_P024B044P = {
    "P024B044P",
    &Panel_P024B044P,
    &p024b044p_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P024B044P */
