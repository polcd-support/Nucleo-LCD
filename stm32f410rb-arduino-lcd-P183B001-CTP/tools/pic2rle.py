#!/usr/bin/env python3
"""把 Image2Lcd 风格的 C 数组转成本工程使用的 RGB565 行程编码资产。

用法:
    python tools/pic2rle.py <输入头文件> <宽> <高> <输出头文件> [--skip 8] [--bpp 3] [--name kLogoImage]

输入文件是参考工程 HARDWARE/LCD/pic.h 那种形式：
    const unsigned char gImage_logo[185648] = { 0x10,0x12,... };

Image2Lcd 生成的数组前若干字节是图像头（含宽高），--skip 指定要跳过的字节数（默认 8）。
--bpp 指定源数据的每像素字节数：
  3（默认）= RGB888，按 8/8/8 转 RGB565；
  2        = 源数据已是 RGB565（大端，与面板一致），直接当作 uint16 读取。
输出为 gfx/image.h 定义的 ScreenImage 结构，每两个 uint16 一组 {像素数, RGB565 颜色}。
"""
import os
import re
import sys


def parse_arrays(path):
    """返回 {符号名: 原始字节}，源文件里可能有多个 gImage_* 数组。"""
    text = open(path, "rb").read()
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = text.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    arrays = {}
    for match in re.finditer(r"(\w+)\s*\[\s*\d+\s*\]\s*=\s*\{", text):
        start = text.index("{", match.start()) + 1
        end = text.index("}", start)
        arrays[match.group(1)] = bytes(int(value, 16) for value in re.findall(r"0[xX]([0-9a-fA-F]+)", text[start:end]))
    return arrays


def pick_array(arrays, expected):
    """优先取满足长度要求的 gImage_logo，其次任意长度匹配的数组。"""
    for name in ("gImage_logo",):
        if name in arrays and len(arrays[name]) == expected:
            return arrays[name]
    for array in arrays.values():
        if len(array) == expected:
            return array
    return None


def to_rgb565(data):
    words = []
    for i in range(0, len(data), 3):
        r, g, b = data[i], data[i + 1], data[i + 2]
        words.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return words


def to_rgb565_raw(data):
    """源数据已经是 RGB565（大端），直接按 uint16 读取。"""
    return [(data[i] << 8) | data[i + 1] for i in range(0, len(data), 2)]


def rle(words):
    runs = []
    previous = words[0]
    count = 1
    for word in words[1:]:
        if word == previous:
            count += 1
        else:
            runs.append((count, previous))
            previous = word
            count = 1
    runs.append((count, previous))
    return runs


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 1
    source, width, height, target = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    skip = 8
    bpp = 3
    name = "kLogoImage"
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    if "--bpp" in sys.argv:
        bpp = int(sys.argv[sys.argv.index("--bpp") + 1])
        if bpp not in (2, 3):
            print("--bpp 只支持 2（RGB565）或 3（RGB888）")
            return 1
    if "--name" in sys.argv:
        name = sys.argv[sys.argv.index("--name") + 1]

    raw = pick_array(parse_arrays(source), skip + width * height * bpp)
    if raw is None:
        print("找不到长度匹配 %dx%dx%d + %d 字节头的数组" % (width, height, bpp, skip))
        return 1
    data = raw[skip:]
    if len(data) != width * height * bpp:
        print("像素数据长度 %d 与 %dx%dx%d 不符（期望 %d）" % (len(data), width, height, bpp, width * height * bpp))
        return 1

    words = to_rgb565_raw(data) if bpp == 2 else to_rgb565(data)
    runs = rle(words)
    runs_name = "k" + name[1:] + "Runs"
    lines = [
        "/*",
        " * %dx%d RGB565 行程编码资产（见 ../gfx/image.h）。" % (width, height),
        " *",
        " * 由 tools/pic2rle.py 从 %s 生成，跳过前 %d 字节的图像头（源 %d 字节/像素）。"
        % (os.path.basename(source), skip, bpp),
        " */",
        '#include "../gfx/image.h"',
        "",
        "static const uint16_t %s[] = {" % runs_name,
    ]
    for i in range(0, len(runs), 6):
        chunk = runs[i:i + 6]
        lines.append("    " + " ".join("%u,0x%04X," % (count, color) for count, color in chunk))
    lines += ["};", "", "static const ScreenImage %s = { %d, %d, %d, %s };" % (name, width, height, len(runs), runs_name), ""]

    open(target, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print("%s: %dx%d, %d 段, 数据 %d 字节（原始 %d 字节/像素 %d 共 %d 字节）"
          % (target, width, height, len(runs), len(runs) * 4, bpp, len(data) // bpp, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
