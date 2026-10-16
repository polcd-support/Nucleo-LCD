/*
 * 屏幕模块的数据模型：面板描述、触摸驱动、设备（面板 + 触摸组合）。
 *
 * 上层只认“设备”，不认具体 IC。新增一块屏 = 在 panels/ 下写一个面板描述
 * （必要时在 touch/ 下写一个触摸驱动），再登记到 screen_devices.h。
 */
#ifndef SCREEN_DEVICE_H
#define SCREEN_DEVICE_H

#include <stdint.h>

/* 面板初始化表的一行：一条命令 + 若干数据字节 + 之后的延时。 */
struct ScreenPanelReg {
    uint8_t command;
    uint8_t length;      /* data 中有效字节数，0 = 只发命令 */
    uint16_t delay_ms;   /* 发完这条之后延时 */
    uint8_t data[16];
};

/* 像素在 SPI 上的字节数：RGB565 = 2，RGB666（18-bit）= 3。 */
#define SCREEN_PIXFMT_RGB565 2
#define SCREEN_PIXFMT_RGB666 3

struct ScreenPanel {
    const char *name;
    uint16_t width;                  /* 原生分辨率（rotation 0，竖屏） */
    uint16_t height;
    const ScreenPanelReg *init;      /* 初始化序列 */
    uint16_t init_count;
    uint8_t madctl[4];               /* 各旋转下的 0x36 值（含 BGR 位） */
    int16_t offset_x[4];             /* 各旋转下 GRAM 列偏移 */
    int16_t offset_y[4];             /* 各旋转下 GRAM 行偏移 */
    uint8_t pixel_format;            /* SCREEN_PIXFMT_* */
    uint32_t spi_hz;                 /* 建议 SPI 时钟 */
    uint8_t spi_mode;                /* 0..3，见 SPISettings */
};

struct ScreenTouchDriver {
    const char *name;
    uint8_t address;
    void (*init)(void);
    /* 返回触点数量；有触点时写入触摸面板原生坐标（未做任何翻转/交换）。 */
    uint8_t (*read)(uint16_t *x, uint16_t *y);
};

struct ScreenTouchConfig {
    const ScreenTouchDriver *driver; /* NULL = 该设备无触摸 */
    uint16_t raw_width;              /* 触摸面板原生坐标范围 */
    uint16_t raw_height;
    uint8_t swap_xy;                 /* 交换 X/Y（触摸方向与屏成 90 度时置 1） */
    uint8_t invert_x;
    uint8_t invert_y;
    int16_t offset_x;                /* 原生坐标修正，最后叠加 */
    int16_t offset_y;
};

struct ScreenDevice {
    const char *name;
    const ScreenPanel *panel;
    const ScreenTouchConfig *touch;  /* NULL = 无触摸 */
};

#endif
