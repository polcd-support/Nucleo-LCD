/*
 * FT6336U / FT6236 电容触摸驱动（硬件 I2C）。
 *
 * 寄存器与参考工程 HARDWARE/FT6236/ft6236.c 一致：
 *   0x02 TD_STATUS  触点数量
 *   0x03..0x06      第 1 点 XH/XL/YH/YL
 *   0x80            触摸阈值
 *   0x88            活动模式上报周期(ms)
 *   0xA3            芯片 ID
 *   0xA4 G_MODE     0 = 轮询，1 = 中断触发
 *   0xA5 POWER_MODE 0 = 活动
 */
#ifndef SCREEN_TOUCH_FT6336U_H
#define SCREEN_TOUCH_FT6336U_H

#include "touch.h"

void Touch_FT6336U_Init(void);
uint8_t Touch_FT6336U_Read(uint16_t *x, uint16_t *y);
uint8_t Touch_FT6336U_ChipId(void);

#endif
