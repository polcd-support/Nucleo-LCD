#include "panel_p028x101.h"

#include "../screen_config.h"
#include "../touch/touch.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P028X101

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 完全一致。
 * 复位（RES 低 100 ms -> 高 100 ms）由 screen.cpp 的 panel_hardware_reset() 完成。 */
static const ScreenPanelReg p028x101_init[] = {
    {0x11, 0, 120, {}},                      /* Sleep Out */
    {0x36, 1, 0, {0x00}},                    /* MADCTL：竖屏，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                    /* 16-bit/pixel（RGB565） */
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}},  /* Porch Setting */
    {0xB7, 1, 0, {0x35}},                    /* Gate Control */
    {0xBB, 1, 0, {0x32}},                    /* VCOM = 1.35 V */
    {0xC2, 1, 0, {0x01}},                    /* VDV/VRH Enable */
    {0xC3, 1, 0, {0x15}},                    /* VRH = 4.8 V */
    {0xC4, 1, 0, {0x20}},                    /* VDV = 0 V */
    {0xC6, 1, 0, {0x0F}},                    /* 60 Hz */
    {0xD0, 2, 0, {0xA4, 0xA1}},              /* Power Control */
    {0xE0, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},  /* 正极性伽马 */
    {0xE1, 14, 0, {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33,
                   0x48, 0x17, 0x14, 0x15, 0x31, 0x34}},  /* 负极性伽马 */
    /* 参考工程此处 read_lcd_id()：0x42C2A9 写 0x20，否则 0x21。
     * 转接板未接 MISO，读不到 ID，固定 0x21。实况反相就改成 0x20。 */
    {0x21, 0, 0, {}},                        /* Display Inversion On */
    {0x29, 0, 0, {}},                        /* Display On */
};

const ScreenPanel Panel_P028X101 = {
    "P028X101 (ST7789 240x320)",
    240,
    320,
    p028x101_init,
    sizeof(p028x101_init) / sizeof(p028x101_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},    /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180 */
    {0, 0, 0, 0},                /* offset_x：GRAM 240x320，无需偏移 */
    {0, 0, 0, 0},                /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                  /* 面板侧上限：ST7789 SPI 可达 ~24 MHz（与板级 24 MHz 一致） */
    0,                           /* 参考工程 SPI1 为 CPOL=0/CPHA=0 -> mode 0 */
};

/* FT6236：参考工程 FT6236 分支不做任何翻转和偏移。 */
static const ScreenTouchConfig p028x101_touch = {
    &Touch_Driver_FT6336U, 240, 320, 0, 0, 0, 0, 0,
};

const ScreenDevice Screen_Device_P028X101 = {
    "P028X101",
    &Panel_P028X101,
    &p028x101_touch,
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P028X101 */
