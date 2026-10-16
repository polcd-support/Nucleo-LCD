#include "panel_p154h007.h"

#include "../screen_config.h"

#if SCREEN_DEVICE == SCREEN_DEVICE_P154H007

/* 初始化序列：顺序与参考工程 HARDWARE/LCD/lcd_init.c 完全一致（逐条转写）。 */
static const ScreenPanelReg p154h007_init[] = {
    {0x11, 0, 120, {}},                      /* Sleep out */
    {0xB2, 5, 0, {0x1F, 0x1F, 0x00, 0x33, 0x33}},  /* Porch control */
    {0x35, 1, 0, {0x00}},                    /* TE on */
    {0x36, 1, 0, {0x00}},                    /* MADCTL：竖屏正向，Screen_SetRotation 会重写 */
    {0x3A, 1, 0, {0x05}},                    /* 像素格式：0x05 = RGB565 */
    {0xB7, 1, 0, {0x00}},                    /* Gate control */
    {0xBB, 1, 0, {0x35}},                    /* VCOM */
    {0xC0, 1, 0, {0x2C}},                    /* Power control 2 */
    {0xC2, 1, 0, {0x01}},                    /* Power control 3 */
    {0xC3, 1, 0, {0x13}},                    /* Power control 4 */
    {0xC4, 1, 0, {0x20}},                    /* Power control 5 */
    {0xC6, 1, 0, {0x13}},                    /* 帧率控制 */
    {0xD6, 1, 0, {0xA1}},
    {0xD0, 2, 10, {0xA7, 0xA1}},             /* Power control 1，参考工程在此延时 10 ms */
    {0xD0, 2, 0, {0xA4, 0xA1}},              /* Power control 1 */
    {0xD6, 1, 0, {0xA1}},
    {0xE0, 14, 0, {0xD0, 0x0E, 0x15, 0x0B, 0x0B, 0x08, 0x30, 0x43,
                   0x44, 0x39, 0x12, 0x12, 0x29, 0x30}},  /* Gamma 正极性 */
    {0xE1, 14, 0, {0xD0, 0x10, 0x12, 0x08, 0x07, 0x13, 0x2F, 0x33,
                   0x44, 0x36, 0x14, 0x14, 0x2B, 0x31}},  /* Gamma 负极性 */
    {0xE4, 3, 0, {0x1D, 0x00, 0x00}},        /* Gate control：(0x1D+1)*8 = 240 gate */
    {0x21, 0, 0, {}},                        /* Display inversion on */
    {0x29, 0, 0, {}},                        /* Display on */
    {0x2A, 4, 0, {0x00, 0x00, 0x00, 0xEF}},  /* 列地址 0~239 */
    {0x2B, 4, 0, {0x00, 0x00, 0x00, 0xEF}},  /* 行地址 0~239 */
    {0x2C, 0, 0, {}},                        /* 储存器写 */
};

const ScreenPanel Panel_P154H007 = {
    "P154H007 (ST7789P3 240x240)",
    240,
    240,
    p154h007_init,
    sizeof(p154h007_init) / sizeof(p154h007_init[0]),
    {0x00, 0xC0, 0x70, 0xA0},   /* 竖屏 / 竖屏 180 / 横屏 / 横屏 180（ST7789 标准值） */
    {0, 0, 0, 0},               /* offset_x：GRAM 240x240，无需偏移 */
    {0, 0, 0, 0},               /* offset_y */
    SCREEN_PIXFMT_RGB565,
    24000000UL,                 /* 面板侧上限（ST7789 可到 ~62 MHz，这里取 24 MHz 高速档） */
    0,                          /* 参考工程 SPI 为 CPOL=0/CPHA=0 */
};

const ScreenDevice Screen_Device_P154H007 = {
    "P154H007",
    &Panel_P154H007,
    nullptr,                    /* 本模组无触摸 */
};

#endif /* SCREEN_DEVICE == SCREEN_DEVICE_P154H007 */
