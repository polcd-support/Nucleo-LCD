/*
 * 面板无关的绘图 API。
 *
 * 移植自参考工程 HARDWARE/LCD/lcd.c（LCD_ShowString/LCD_ShowChinese/画线等），
 * 差异：
 *  - 颜色统一用 RGB565，由 screen 层按面板格式转换。
 *  - 逐像素写显存的写法改成“先把一个字模/一行数据装进缓冲，再一次性写窗口”，
 *    大幅减少 SPI 事务数，效果相同。
 *  - 中文索引按 UTF-8 三字节匹配，源码用 UTF-8 直接写中文即可。
 */
#ifndef SCREEN_GFX_GFX_H
#define SCREEN_GFX_GFX_H

#include <stdint.h>

#include "image.h"

/* RGB565 调色板，取值与参考工程 lcd.h 一致（CYAN 是 0x7FFF，彩条依赖这个值）。 */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0xF81F
#define GRED 0xFFE0
#define GBLUE 0x07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0xBC40
#define BRRED 0xFC07
#define GRAY 0x8430
#define DARKBLUE 0x01CF
#define LIGHTBLUE 0x7D7C
#define GRAYBLUE 0x5458
#define LIGHTGREEN 0x841F
#define LGRAY 0xC618
#define LGRAYBLUE 0xA651
#define LBBLUE 0x2B12

#define RGB565(r, g, b) ((uint16_t)(((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | ((uint16_t)(b) >> 3))

/* 参考工程里手指采样点间距超过该值就丢弃该线段，这里保留同样行为。 */
#define LCD_MAX_SEGMENT 50

void LCD_Fill(uint16_t color);
void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t length, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);
void LCD_DrawThickLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint8_t size);

/* 字号 sizey 取 12/16/24/32（字符宽 = sizey/2）。 */
void LCD_ShowChar(uint16_t x, uint16_t y, char character, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowString(uint16_t x, uint16_t y, const char *text, uint16_t fc, uint16_t bc, uint8_t sizey);
/* 中文字库只含参考工程里取模过的几个汉字；字符串必须按 UTF-8 编码。 */
void LCD_ShowChinese(uint16_t x, uint16_t y, const char *text, uint16_t fc, uint16_t bc, uint8_t sizey);

/* 绘制 RLE 图像（见 image.h）。 */
void LCD_ShowImage(uint16_t x, uint16_t y, const ScreenImage *image);

#endif
