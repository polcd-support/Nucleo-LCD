/*
 * RLE 图像容器。
 *
 * 屏幕整屏的 RGB565 原始数据动辄上百 KB，占用单片机有限的 Flash（Nucleo-F410RB 128 KB），
 * 因此图像资产一律用行程编码存放：每两个 uint16 一组，依次为
 *   { 像素个数, RGB565 颜色 }
 * 按行优先顺序展开。
 *
 * 本工程 demo 不显示 LOGO（参考工程里该状态不可达），image.h / LCD_ShowImage()
 * 与 tools/pic2rle.py 作为绘图库与工具链能力保留，供需要时使用。
 *
 * 生成工具：tools/pic2rle.py（--bpp 3 处理 RGB888，--bpp 2 处理 RGB565 源数据）
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
