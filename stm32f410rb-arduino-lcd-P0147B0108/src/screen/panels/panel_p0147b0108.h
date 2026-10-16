/*
 * ST7789P3 面板（P0147B01-08 BOE 1.47 寸 172x320）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 规格书 board/ST7789P3+BOE1.47.txt 确认为 ST7789P3。
 * 要点：
 * - 172x320 有效像素，GRAM 为 240x320，竖屏时列方向需偏移 34（在 240 列里居中）。
 * - 0x3A = 0x05，16-bit/pixel（RGB565），每像素 2 字节。
 * - 0x36 (MADCTL) 四种方向值与参考工程一致：0x00/0xC0 竖屏，0x70/0xA0 横屏。
 * - 参考工程 SPI1 为 64 MHz PCLK 下 2 分频（约 32 MHz），此处面板侧取 20 MHz 上限。
 */
#ifndef SCREEN_PANEL_P0147B0108_H
#define SCREEN_PANEL_P0147B0108_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P0147B0108;
extern const ScreenDevice Screen_Device_P0147B0108;

#endif
