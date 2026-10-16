/*
 * ST7789 面板（P183B001-CTP 2.4 寸 240x284 IPS）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏）。
 * 要点：
 * - 0x3A = 0x05 即 16-bit/pixel（RGB565），SPI 每像素 2 字节。
 * - 0x36 (MADCTL) 四种方向值与参考工程一致：0x00/0xC0 竖屏，0x70/0xA0 横屏。
 * - 参考工程 LCD_Address_Set() 四个分支都没有行列偏移，GRAM 与可见区一致。
 */
#ifndef SCREEN_PANEL_P183B001_CTP_H
#define SCREEN_PANEL_P183B001_CTP_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P183B001_CTP;
extern const ScreenDevice Screen_Device_P183B001_CTP;

#endif
