/*
 * 字模表声明（由 STM32 参考工程 lcdfont.h 转换而来）。
 */
#ifndef SCREEN_GFX_FONTS_H
#define SCREEN_GFX_FONTS_H

#include <stdint.h>

/* 中文点阵结构：Index 是 UTF-8 三字节索引，数组留 4 字节容纳字符串字面量的结尾 0，
 * 查找时只比较前 3 字节。 */
typedef struct {
    unsigned char Index[4];
    unsigned char Msk[24];
} typFNT_GB12;

typedef struct {
    unsigned char Index[4];
    unsigned char Msk[32];
} typFNT_GB16;

typedef struct {
    unsigned char Index[4];
    unsigned char Msk[72];
} typFNT_GB24;

typedef struct {
    unsigned char Index[4];
    unsigned char Msk[128];
} typFNT_GB32;

extern const unsigned char ascii_1206[][12];
extern const unsigned char ascii_1608[][16];
extern const unsigned char ascii_2412[][48];
extern const unsigned char ascii_3216[][64];
extern const typFNT_GB12 tfont12[];
extern const typFNT_GB16 tfont16[];
extern const typFNT_GB24 tfont24[];
extern const typFNT_GB32 tfont32[];

extern const uint16_t FONT_CN12_COUNT;
extern const uint16_t FONT_CN16_COUNT;
extern const uint16_t FONT_CN24_COUNT;
extern const uint16_t FONT_CN32_COUNT;

#endif
