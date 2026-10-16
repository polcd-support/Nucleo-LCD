/*
 * 屏幕模块对外接口。上层（gfx、app）只调用这里，不直接碰 SPI/面板寄存器。
 *
 * 一次典型的绘制：
 *     Screen_SetWindow(x, y, x + w - 1, y + h - 1);
 *     Screen_WritePixels565(pixels, w * h);      // 由它释放 CS
 * 或者直接用 Screen_FillRect / Screen_Fill。
 */
#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

#include "screen_config.h"

/* 上电初始化：配置引脚电平 -> 使能电平转换器 -> 面板复位/初始化 -> 触摸复位/初始化。
 * 背光保持关闭，由调用方在画面准备好之后再打开。成功返回 true。 */
bool Screen_Init(void);

const char *Screen_Name(void);
uint16_t Screen_Width(void);
uint16_t Screen_Height(void);
uint8_t Screen_Rotation(void);
void Screen_SetRotation(uint8_t rotation);

/* 背光：on = 全亮。Level 0..255，仅当板卡定义 SCREEN_BL_PWM 时真正调光。 */
void Screen_Backlight(bool on);
void Screen_BacklightLevel(uint8_t level);

/* 设置写窗口。调用后 CS 保持有效，必须由 Screen_WritePixels565() 结束。 */
void Screen_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
/* 写入 count 个 RGB565 像素（自动按面板像素格式转换并分块发送）。 */
void Screen_WritePixels565(const uint16_t *pixels, uint32_t count);

void Screen_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void Screen_Fill(uint16_t color);

bool Screen_HasTouch(void);
/* 触摸坐标，已换算到当前显示方向；返回触点数量（0 = 无触点）。 */
uint8_t Screen_TouchRead(uint16_t *x, uint16_t *y);
/* 触摸面板原生坐标，用于标定 ScreenTouchConfig 的翻转/交换/偏移。 */
uint8_t Screen_TouchReadRaw(uint16_t *x, uint16_t *y);

#endif
