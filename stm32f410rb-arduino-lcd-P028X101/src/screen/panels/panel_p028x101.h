/*
 * ST7789 面板（P028X101 2.8 寸 240x320 TFT）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 2.8 寸 240x320 SPI 屏基本是 ST7789/ST7789V。要点：
 * - 0x3A = 0x05，16-bit/pixel（RGB565），SPI 下每像素 2 字节。
 * - 参考工程 0x36 (MADCTL) 硬编码 0x00，无法从源码看出另外三个方向；
 *   这里按 ST7789 标准补全为 0x00/0xC0/0x70/0xA0，README 有说明。
 * - GRAM 为 240x320，四个方向都不需要行列偏移。
 * - 参考工程初始化末尾用 read_lcd_id() 决定写 0x20 还是 0x21；UNO R4 转接板
 *   没有接 MISO，读不到 ID，这里固定写 0x21（见 README）。
 */
#ifndef SCREEN_PANEL_P028X101_H
#define SCREEN_PANEL_P028X101_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P028X101;
extern const ScreenDevice Screen_Device_P028X101;

#endif
