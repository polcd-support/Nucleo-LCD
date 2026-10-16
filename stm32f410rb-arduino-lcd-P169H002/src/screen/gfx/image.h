/*
 * RLE 图像容器。
 *
 * 屏幕整屏的 RGB565 原始数据往往超过 UNO R4 的 245 KB 可用 Flash
 * （240x280 就有 134 KB），因此资产一律用行程编码存放：每两个 uint16 一组，依次为
 *   { 像素个数, RGB565 颜色 }
 * 按行优先顺序展开。本工程的 LOGO 只有 3522 段，压缩后 14 088 字节。
 *
 * 生成工具：tools/pic2rle.py
 */
#ifndef SCREEN_GFX_IMAGE_H
#define SCREEN_GFX_IMAGE_H

#include <stdint.h>

struct ScreenImage {
    uint16_t width;
    uint16_t height;
    uint32_t run_count;        /* 行程数，runs 数组长度为 run_count*2 */
    const uint16_t *runs;      /* {像素数, 颜色} 交替 */
};

#endif
