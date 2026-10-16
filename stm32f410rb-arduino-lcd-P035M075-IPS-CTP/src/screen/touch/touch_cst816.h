/*
 * CST816S/T 电容触摸驱动（硬件 I2C）。
 *
 * 寄存器取自参考工程 HARDWARE/CST816/CST816.c：地址 0x15，
 * 0x02 触点数量，0x03..0x06 坐标，0xA7 芯片 ID。
 * 参考工程对该模组额外加 15 像素 Y 偏移，这里改由 ScreenTouchConfig.offset_y 表达。
 */
#ifndef SCREEN_TOUCH_CST816_H
#define SCREEN_TOUCH_CST816_H

#include "touch.h"

void Touch_CST816_Init(void);
uint8_t Touch_CST816_Read(uint16_t *x, uint16_t *y);
uint8_t Touch_CST816_ChipId(void);

#endif
