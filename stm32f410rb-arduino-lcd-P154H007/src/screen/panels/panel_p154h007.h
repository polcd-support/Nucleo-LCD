/*
 * ST7789P3 面板（P154H007 1.54 寸 240x240 IPS）。
 *
 * 初始化序列取自参考工程 HARDWARE/LCD/lcd_init.c（该工程就是用这套寄存器点这块屏），
 * 规格书为 ST7789P3-HSD1.54IPS。要点：
 * - 0x3A 写 0x05，参考工程注释即“0x05 = RGB565”，每像素 2 字节。
 * - 0x36 (MADCTL) 参考工程硬编码为 0x00（竖屏正向），四个方向取 ST7789 标准值
 *   0x00/0xC0/0x70/0xA0，测试与 README 均按此说明。
 * - GRAM 为 240x240，四个方向都不需要行列偏移。
 * - 参考工程初始化序列末尾会写 0x2A/0x2B（0..239）与 0x2C，这里照抄。
 * - 本模组无触摸（参考工程 HARDWARE/CST816、FT6236 存在但 demo 完全不用）。
 */
#ifndef SCREEN_PANEL_P154H007_H
#define SCREEN_PANEL_P154H007_H

#include "../screen_device.h"

extern const ScreenPanel Panel_P154H007;
extern const ScreenDevice Screen_Device_P154H007;

#endif
