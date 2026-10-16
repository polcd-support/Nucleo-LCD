#include "gfx.h"

#include <string.h>

#include "../screen.h"
#include "fonts.h"

/* 单个字模最多 32x32 像素。 */
#define GFX_GLYPH_PIXELS (32 * 32)
/* 图像解码/送显的暂存像素数，与 6 行 RGB666 发送块对齐。 */
#define GFX_STAGE_PIXELS 1920

static uint16_t s_glyph[GFX_GLYPH_PIXELS];
static uint16_t s_stage[GFX_STAGE_PIXELS];

/* ------------------------------------------------------------------ 面 */

/* 有符号的水平填充：圆心减半径会算出负值，Uint16 直传会绕回成大数。 */
static void fill_span(int32_t x0, int32_t x1, int32_t y, uint16_t color)
{
    if (y < 0 || y >= (int32_t)Screen_Height()) return;
    if (x0 > x1) { const int32_t t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= (int32_t)Screen_Width()) x1 = (int32_t)Screen_Width() - 1;
    if (x0 > x1) return;
    Screen_FillRect((uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y, color);
}

void LCD_Fill(uint16_t color)
{
    Screen_Fill(color);
}

void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    Screen_FillRect(x0, y0, x1, y1, color);
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    Screen_FillRect(x, y, x, y, color);
}

void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t length, uint16_t color)
{
    if (length == 0) return;
    Screen_FillRect(x, y, (uint16_t)(x + length - 1), y, color);
}

/* ------------------------------------------------------------------ 线 */

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int32_t x = x1;
    int32_t y = y1;
    const int32_t dx = (int32_t)x2 - (int32_t)x1;
    const int32_t dy = (int32_t)y2 - (int32_t)y1;
    const int32_t step_x = dx >= 0 ? 1 : -1;
    const int32_t step_y = dy >= 0 ? 1 : -1;
    const int32_t abs_dx = dx >= 0 ? dx : -dx;
    const int32_t abs_dy = dy >= 0 ? dy : -dy;
    int32_t err = abs_dx - abs_dy;

    for (;;) {
        if (x >= 0 && y >= 0 && x < (int32_t)Screen_Width() && y < (int32_t)Screen_Height()) {
            Screen_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y, color);
        }
        if (x == (int32_t)x2 && y == (int32_t)y2) break;
        const int32_t e2 = 2 * err;
        if (e2 > -abs_dy) { err -= abs_dy; x += step_x; }
        if (e2 < abs_dx) { err += abs_dx; y += step_y; }
    }
}

/* 矩形边框：四条边，与参考工程 LCD_DrawRectangle() 相同。 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
}

void LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    if (radius == 0) return;

    const int32_t cx = x0;
    const int32_t cy = y0;
    int32_t x = radius;
    int32_t y = 0;
    int32_t err = 0;

    while (x >= y) {
        fill_span(cx - x, cx + x, cy + y, color);
        fill_span(cx - y, cx + y, cy + x, color);
        fill_span(cx - x, cx + x, cy - y, color);
        fill_span(cx - y, cx + y, cy - x, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void LCD_DrawThickLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint8_t size)
{
    if (size <= 1) {
        LCD_DrawLine(x1, y1, x2, y2, color);
        return;
    }

    /* 与参考工程同样的保护：贴着边缘的点、以及跨度超过 LCD_MAX_SEGMENT 的线段不画。 */
    if (x1 < size || x2 < size || y1 < size || y2 < size) return;

    const int32_t dx = (int32_t)x2 - (int32_t)x1;
    const int32_t dy = (int32_t)y2 - (int32_t)y1;
    const uint16_t abs_dx = (uint16_t)(dx >= 0 ? dx : -dx);
    const uint16_t abs_dy = (uint16_t)(dy >= 0 ? dy : -dy);
    if (abs_dx > LCD_MAX_SEGMENT || abs_dy > LCD_MAX_SEGMENT) return;

    int32_t x = x1;
    int32_t y = y1;
    const int32_t step_x = dx >= 0 ? 1 : -1;
    const int32_t step_y = dy >= 0 ? 1 : -1;
    const int32_t adx = abs_dx;
    const int32_t ady = abs_dy;
    int32_t err = adx - ady;

    for (;;) {
        LCD_FillCircle((uint16_t)x, (uint16_t)y, size, color);
        if (x == (int32_t)x2 && y == (int32_t)y2) break;
        const int32_t e2 = 2 * err;
        if (e2 > -ady) { err -= ady; x += step_x; }
        if (e2 < adx) { err += adx; y += step_y; }
    }
}

/* ------------------------------------------------------------------ 字 */

static const uint8_t *ascii_table(uint8_t sizey, uint16_t *bytes_per_char)
{
    switch (sizey) {
        case 12: *bytes_per_char = 12; return (const uint8_t *)ascii_1206;
        case 16: *bytes_per_char = 16; return (const uint8_t *)ascii_1608;
        case 24: *bytes_per_char = 48; return (const uint8_t *)ascii_2412;
        case 32: *bytes_per_char = 64; return (const uint8_t *)ascii_3216;
        default: return 0;
    }
}

/* 与参考工程一致的点阵顺序：每行占 (sizex+7)/8 字节，位 0 是最左像素。 */
static void unpack(const uint8_t *data, uint16_t width, uint16_t height, uint16_t fc, uint16_t bc)
{
    const uint16_t bytes_per_row = (uint16_t)((width + 7) / 8);
    uint16_t *out = s_glyph;

    for (uint16_t row = 0; row < height; row++) {
        for (uint16_t b = 0; b < bytes_per_row; b++) {
            const uint8_t bits = data[row * bytes_per_row + b];
            for (uint16_t bit = 0; bit < 8; bit++) {
                if ((uint16_t)(b * 8 + bit) >= width) break;
                *out++ = (bits & (1u << bit)) ? fc : bc;
            }
        }
    }
}

void LCD_ShowChar(uint16_t x, uint16_t y, char character, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    const uint8_t index = (uint8_t)character;
    if (index < ' ' || index > 0x7F) return;

    uint16_t bytes_per_char = 0;
    const uint8_t *table = ascii_table(sizey, &bytes_per_char);
    if (!table) return;

    const uint16_t sizex = (uint16_t)(sizey / 2);
    if ((uint32_t)x + sizex > Screen_Width() || (uint32_t)y + sizey > Screen_Height()) return;

    unpack(table + (uint32_t)(index - ' ') * bytes_per_char, sizex, sizey, fc, bc);
    Screen_SetWindow(x, y, (uint16_t)(x + sizex - 1), (uint16_t)(y + sizey - 1));
    Screen_WritePixels565(s_glyph, (uint32_t)sizex * sizey);
}

void LCD_ShowString(uint16_t x, uint16_t y, const char *text, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    if (sizey != 12 && sizey != 16 && sizey != 24 && sizey != 32) return;
    while (*text != '\0') {
        LCD_ShowChar(x, y, *text, fc, bc, sizey);
        x = (uint16_t)(x + sizey / 2);
        text++;
    }
}

/* 10 的 n 次幂（n <= 4 足够覆盖 uint16 的位数）。 */
static uint32_t pow10_u32(uint8_t n)
{
    uint32_t result = 1;
    while (n-- > 0) result *= 10;
    return result;
}

void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    if (sizey != 12 && sizey != 16 && sizey != 24 && sizey != 32) return;
    const uint16_t sizex = (uint16_t)(sizey / 2);
    bool enshow = false;
    for (uint8_t t = 0; t < len; t++) {
        const uint8_t digit = (uint8_t)((num / pow10_u32((uint8_t)(len - t - 1))) % 10);
        /* 与参考工程一致：遇到第一个非 0 位之前的高位补空格，最后一位即使是 0 也照常显示。 */
        if (!enshow && t < (uint8_t)(len - 1)) {
            if (digit == 0) {
                LCD_ShowChar((uint16_t)(x + t * sizex), y, ' ', fc, bc, sizey);
                continue;
            }
            enshow = true;
        }
        LCD_ShowChar((uint16_t)(x + t * sizex), y, (char)('0' + digit), fc, bc, sizey);
    }
}

void LCD_ShowChinese(uint16_t x, uint16_t y, const char *text, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    const uint8_t *base = 0;
    uint16_t entry_size = 0;
    uint16_t count = 0;

    switch (sizey) {
        case 12: base = (const uint8_t *)tfont12; entry_size = sizeof(typFNT_GB12); count = FONT_CN12_COUNT; break;
        case 16: base = (const uint8_t *)tfont16; entry_size = sizeof(typFNT_GB16); count = FONT_CN16_COUNT; break;
        case 24: base = (const uint8_t *)tfont24; entry_size = sizeof(typFNT_GB24); count = FONT_CN24_COUNT; break;
        case 32: base = (const uint8_t *)tfont32; entry_size = sizeof(typFNT_GB32); count = FONT_CN32_COUNT; break;
        default: return;
    }

    while ((uint8_t)*text >= 0x80) {
        const uint8_t *glyph = 0;
        for (uint16_t k = 0; k < count; k++) {
            const uint8_t *entry = base + (uint32_t)k * entry_size;
            if (entry[0] == (uint8_t)text[0] && entry[1] == (uint8_t)text[1] && entry[2] == (uint8_t)text[2]) {
                glyph = entry + 4;   /* Index 之后即点阵数据 */
                break;
            }
        }
        if (glyph && (uint32_t)x + sizey <= Screen_Width() && (uint32_t)y + sizey <= Screen_Height()) {
            unpack(glyph, sizey, sizey, fc, bc);
            Screen_SetWindow(x, y, (uint16_t)(x + sizey - 1), (uint16_t)(y + sizey - 1));
            Screen_WritePixels565(s_glyph, (uint32_t)sizey * sizey);
        }
        text += 3;
        x = (uint16_t)(x + sizey);
    }
}

/* ------------------------------------------------------------------ 图 */

void LCD_ShowImage(uint16_t x, uint16_t y, const ScreenImage *image)
{
    if (!image || image->width == 0 || image->height == 0) return;

    const uint16_t w = image->width;
    const uint16_t h = image->height;
    if ((uint32_t)x + w > Screen_Width() || (uint32_t)y + h > Screen_Height()) return;

    Screen_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    const uint16_t *runs = image->runs;
    uint32_t staged = 0;
    for (uint32_t i = 0; i < image->run_count; i++) {
        uint32_t length = runs[i * 2];
        const uint16_t color = runs[i * 2 + 1];
        while (length > 0) {
            uint32_t take = GFX_STAGE_PIXELS - staged;
            if (take > length) take = length;
            for (uint32_t k = 0; k < take; k++) s_stage[staged + k] = color;
            staged += take;
            length -= take;
            if (staged == GFX_STAGE_PIXELS) {
                Screen_WritePixels565(s_stage, staged);
                staged = 0;
            }
        }
    }
    if (staged > 0) Screen_WritePixels565(s_stage, staged);
}
