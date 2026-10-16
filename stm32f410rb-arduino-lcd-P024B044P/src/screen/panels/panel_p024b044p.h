/*
 * ST7789 面板（P024B044P 2.4 寸 240x320 IPS）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 规格书确认为 ST7789。要点：
 * - 0x3A = 0x05，16-bit/pixel（RGB565，每像素 2 字节）。
 * - 0x36 (MADCTL)：参考工程写死 0x00，只支持方向 0；横屏值按 ST7789 标准补全为
 *   0xC0/0x70/0xA0，实物核对前请先只在方向 0 下用。
 * - GRAM 为 240x320，四个方向都不需要行列偏移。
 * - 初始化序列末尾在参考工程里有一段需要读回 LCD ID 的运行时分支（read_lcd_id() 走
 *   MISO）；UNO R4 + 转接板没有回读线，这里硬编码为 0x21（Display Inversion On）。
 */
#ifndef SCREEN_PANEL_P024B044P_H
#define SCREEN_PANEL_P024B044P_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P024B044P;
extern const ScreenDevice Screen_Device_P024B044P;

#endif
