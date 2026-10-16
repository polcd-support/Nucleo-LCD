#!/usr/bin/env python3
"""把 Image2Lcd 风格的 C 数组转成本工程使用的 RGB565 行程编码资产。

用法:
    python tools/pic2rle.py <输入头文件> <宽> <高> <输出头文件> [--skip 8] [--bpp 3]
                            [--name kLogoImage] [--array gImage_logo]

输入文件是参考工程 HARDWARE/LCD/pic.h 那种形式：
    const unsigned char gImage_logo[185648] = { 0x10,0x12,... };

Image2Lcd 生成的数组前若干字节是图像头（含宽高），--skip 指定要跳过的字节数（默认 8）。
--bpp 指定像素字节数：3 = RGB888（默认，向后兼容），2 = RGB565（高位在前）。
一个头文件里可能有多个数组（如 gImage_img1/img2/img3/logo）：不给 --array 时，
按“去掉图像头后的字节数 == 宽 x 高 x bpp”自动挑选匹配的那个。
输出为 gfx/image.h 定义的 ScreenImage 结构，每两个 uint16 一组 {像素数, RGB565 颜色}。
"""
import re
import sys

ARRAY_RE = re.compile(r"([A-Za-z_]\w*)\s*\[(\d+)\]\s*=\s*\{")


def parse_arrays(path):
    """返回 {数组名: bytes}，容忍 GBK/UTF-8 源码。"""
    raw = open(path, "rb").read()
    text = None
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = raw.decode(encoding)
            break
        except UnicodeDecodeError:
            continue

    arrays = {}
    for match in ARRAY_RE.finditer(text):
        name = match.group(1)
        end = text.index("}", match.end())
        body = text[match.end():end]
        arrays[name] = bytes(int(value, 16) for value in re.findall(r"0[xX]([0-9a-fA-F]+)", body))
    return arrays


def to_rgb565(data):
    words = []
    for i in range(0, len(data), 2):
        words.append((data[i] << 8) | data[i + 1])
    return words


def to_rgb565_from_rgb888(data):
    words = []
    for i in range(0, len(data), 3):
        r, g, b = data[i], data[i + 1], data[i + 2]
        words.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return words


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
    array_name = None
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    if "--bpp" in sys.argv:
        bpp = int(sys.argv[sys.argv.index("--bpp") + 1])
    if "--name" in sys.argv:
        name = sys.argv[sys.argv.index("--name") + 1]
    if "--array" in sys.argv:
        array_name = sys.argv[sys.argv.index("--array") + 1]

    arrays = parse_arrays(source)
    if array_name is not None:
        if array_name not in arrays:
            print("找不到数组 %s（可用：%s）" % (array_name, ", ".join(arrays)))
            return 1
        raw = arrays[array_name]
    else:
        expect = width * height * bpp
        matches = [n for n, d in arrays.items() if len(d) - skip == expect]
        if len(matches) == 1:
            raw = arrays[matches[0]]
        elif len(matches) == 0:
            print("没有数组在跳过 %d 字节后长度等于 %dx%dx%d=%d（各数组：%s）"
                  % (skip, width, height, bpp, expect,
                     ", ".join("%s=%d" % (n, len(d)) for n, d in arrays.items())))
            return 1
        else:
            print("有多个数组匹配 %s，请用 --array 指定" % ", ".join(matches))
            return 1

    data = raw[skip:]
    if len(data) != width * height * bpp:
        print("像素数据长度 %d 与 %dx%dx%d 不符（期望 %d）"
              % (len(data), width, height, bpp, width * height * bpp))
        return 1

    words = to_rgb565(data) if bpp == 2 else to_rgb565_from_rgb888(data)
    runs = rle(words)
    runs_name = "k" + name[1:] + "Runs"
    lines = [
        "/*",
        " * %dx%d RGB565 行程编码资产（见 ../gfx/image.h）。" % (width, height),
        " *",
        " * 由 tools/pic2rle.py 从 %s 生成，跳过前 %d 字节的图像头（输入 %d 字节/像素）。" % (source, skip, bpp),
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
    print("%s: %dx%d, %d 段, 数据 %d 字节（原始 %d 字节/像素 %d 字节）"
          % (target, width, height, len(runs), len(runs) * 4, bpp, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
