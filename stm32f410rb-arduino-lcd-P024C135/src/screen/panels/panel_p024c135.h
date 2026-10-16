/*
 * ILI9341 面板（P024C135 2.4 寸 240x320）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 序列里的 0xCF/0xED/0xE8/0xCB/0xF7/0xEA/C0/C1/C5/C7/B1/B6/F2/26/E0/E1 是 ILI9341 的典型值。
 * 要点：
 * - 0x3A = 0x55，16-bit/pixel（RGB565），每像素 2 字节。
 * - 0x36 (MADCTL) 四种方向值与参考工程一致：0x08/0xC8 竖屏，0x78/0xA8 横屏，都带 BGR 位。
 * - GRAM 为 240x320，四个方向都不需要行列偏移。
 */
#ifndef SCREEN_PANEL_P024C135_H
#define SCREEN_PANEL_P024C135_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P024C135;
extern const ScreenDevice Screen_Device_P024C135;

#endif
