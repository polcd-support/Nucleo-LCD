/*
 * ST7789 面板（P169H002，1.69 寸 240x280 IPS，带 CST816 电容触摸）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 逐条转写，未增删任何命令或数据字节。要点：
 * - 0x3A = 0x05，即 16-bit/pixel（RGB565），每像素 2 字节。
 * - 0x36 (MADCTL) 四个方向值与参考工程一致：0x00/0xC0 竖屏，0x70/0xA0 横屏。
 * - GRAM 为 240x320，可见区 240x280；参考工程的 LCD_Address_Set() 里给了各方向的偏移，
 *   这里逐字照抄（见 .cpp 的注释）。
 */
#ifndef SCREEN_PANEL_P169H002_H
#define SCREEN_PANEL_P169H002_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P169H002;
extern const ScreenDevice Screen_Device_P169H002;

#endif
