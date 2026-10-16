/*
 * ILI9488 面板（P035M075-IPS-CTP 3.5 寸 320x480 IPS）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 规格书第 2 页确认为 ILI9488。要点：
 * - 4 线 SPI 只能工作 18-bit（RGB666）模式，0x3A = 0x66，每像素 3 字节。
 *   ILI9488 的 SPI 接口不支持 16-bit 像素，用 RGB565 会出现花屏/错色。
 * - 0x36 (MADCTL) 四种方向值与参考工程一致：0x48/0x88 竖屏，0x28/0xE8 横屏，
 *   都带 BGR 位。
 * - GRAM 为 320x480，四个方向都不需要行列偏移。
 */
#ifndef SCREEN_PANEL_P035M075_IPS_CTP_H
#define SCREEN_PANEL_P035M075_IPS_CTP_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P035M075_IPS_CTP;
extern const ScreenDevice Screen_Device_P035M075_IPS_CTP;

#endif
