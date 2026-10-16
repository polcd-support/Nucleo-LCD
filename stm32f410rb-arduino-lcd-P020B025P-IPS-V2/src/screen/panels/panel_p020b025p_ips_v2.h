/*
 * ST7789P3 面板（P020B025P-IPS-V2 2.0 寸 240x320 IPS，无触摸）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 源码注释写明为 BOE 2.0-inch ST7789P3。要点：
 * - 0x3A = 0x05：16-bit/pixel（RGB565），每像素 2 字节。
 * - 0x36 (MADCTL) 四种方向值与参考工程一致：0x00/0xC0 竖屏，0x70/0xA0 横屏。
 * - GRAM 为 240x320，四个方向都不需要行列偏移。
 */
#ifndef SCREEN_PANEL_P020B025P_IPS_V2_H
#define SCREEN_PANEL_P020B025P_IPS_V2_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P020B025P_IPS_V2;
extern const ScreenDevice Screen_Device_P020B025P_IPS_V2;

#endif
