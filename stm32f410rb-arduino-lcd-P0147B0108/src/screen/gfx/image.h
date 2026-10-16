/*
 * RLE 图像容器。
 *
 * 屏幕整屏 172x320 的 RGB565 原始数据是 110 KB，加上字库会挤占 UNO R4 的
 * 245 KB 可用 Flash，因此资产一律用行程编码存放：每两个 uint16 一组，依次为
 *   { 像素个数, RGB565 颜色 }
 * 按行优先顺序展开，适合大面积同色的图形（LOGO、图标等）。
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
