#include "panel_p024c135.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P024C135

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 的 LCD_Init() 完全一致。 */
static const ScreenPanelReg p024c135_init[] = {
    {0x11, 0, 120, {}},                      /* Sleep Out，之后延时 120 ms */
    {0xCF, 3, 0, {0x00, 0xD9, 0x30}},        /* Power Control B */
    {0xED, 4, 0, {0x64, 0x03, 0x12, 0x81}},  /* Power On Sequence Control */
    {0xE8, 3, 0, {0x85, 0x10, 0x78}},        /* Driver Timing Control A */
    {0xCB, 5, 0, {0x39, 0x2C, 0x00, 0x34, 0x02}},  /* Power Control A */
    {0xF7, 1, 0, {0x20}},                    /* Pump Ratio Control */
    {0xEA, 2, 0, {0x00, 0x00}},              /* Driver Timing Control B */
    {0xC0, 1, 0, {0x21}},                    /* Power Control 1（VRH[5:0]） */
    {0xC1, 1, 0, {0x12}},                    /* Power Control 2（SAP/BT） */
    {0xC5, 2, 0, {0x32, 0x3C}},              /* VCOM Control */
    {0xC7, 1, 0, {0xC1}},                    /* VCOM Control 2 */
    {0x36, 1, 0, {0x08}},                    /* MADCTL：竖屏 + BGR，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x55}},                    /* 16-bit/pixel（RGB565） */
    {0xB1, 2, 0, {0x00, 0x18}},              /* Frame Rate Control */
    {0xB6, 2, 0, {0x0A, 0xA2}},              /* Display Function Control */
    {0xF2, 1, 0, {0x00}},                    /* 3Gamma Function Disable */
    {0x26, 1, 0, {0x01}},                    /* Gamma curve selected */
    {0xE0, 15, 0, {0x0F, 0x20, 0x1E, 0x09, 0x12, 0x0B, 0x50, 0xBA,
                   0x44, 0x09, 0x14, 0x05, 0x23, 0x21, 0x00}},  /* 正极性伽马 */
    {0xE1, 15, 0, {0x00, 0x19, 0x19, 0x00, 0x12, 0x07, 0x2D, 0x28,
                   0x3F, 0x02, 0x0A, 0x08, 0x25, 0x2D, 0x0F}},  /* 负极性伽马 */
    {0x29, 0, 0, {}},                        /* Display On */
};

const ScreenPanel Panel_P024C135 = {
    "P024C135 (ILI9341 240x320)",
    240,
    320,
    p024c135_init,
    sizeof(p024c135_init) / sizeof(p024c135_init[0]),
    {0x08, 0xC8, 0x78, 0xA8},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180（参考工程 0x36 的四档值） */
    {0, 0, 0, 0},               /* offset_x：参考工程 LCD_Address_Set() 无偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB565,
    10000000UL,                 /* 面板侧上限：ILI9341 4 线写时钟 10 MHz */
    0,                          /* 参考工程 SPI 为 CPOL=0/CPHA=0（LOW + 1EDGE） */
};

/* FT6236：参考工程不做任何翻转和偏移（FT6xxx 寄存器，兼容 FT6336U）。 */
static const ScreenTouchConfig p024c135_touch = {
    &Touch_Driver_FT6336U, 240, 320, 0, 0, 0, 0, 0,
};

const ScreenDevice Screen_Device_P024C135 = {
    "P024C135",
    &Panel_P024C135,
    &p024c135_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P024C135 */
