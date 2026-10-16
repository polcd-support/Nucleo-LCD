#!/usr/bin/env python3
"""把 Image2Lcd 风格的 C 数组（RGB888）转成本工程使用的 RGB565 行程编码资产。

用法:
    python tools/pic2rle.py <输入头文件> <宽> <高> <输出头文件> [--skip 8] [--name kLogoImage]

输入文件是参考工程 HARDWARE/LCD/pic.h 那种形式：
    const unsigned char gImage_logo[185648] = { 0x10,0x12,... };

Image2Lcd 生成的数组前若干字节是图像头（含宽高），--skip 指定要跳过的字节数（默认 8）。
输出为 gfx/image.h 定义的 ScreenImage 结构，每两个 uint16 一组 {像素数, RGB565 颜色}。
"""
import re
import sys


def parse_array(path):
    text = open(path, "rb").read()
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = text.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    body = text[text.index("{") + 1:text.rindex("}")]
    return bytes(int(value, 16) for value in re.findall(r"0[xX]([0-9a-fA-F]+)", body))


def to_rgb565(data):
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
    name = "kLogoImage"
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    if "--name" in sys.argv:
        name = sys.argv[sys.argv.index("--name") + 1]

    data = parse_array(source)[skip:]
    if len(data) != width * height * 3:
        print("像素数据长度 %d 与 %dx%d 不符（期望 %d）" % (len(data), width, height, width * height * 3))
        return 1

    runs = rle(to_rgb565(data))
    runs_name = "k" + name[1:] + "Runs"
    lines = [
        "/*",
        " * %dx%d RGB565 行程编码资产（见 ../gfx/image.h）。" % (width, height),
        " *",
        " * 由 tools/pic2rle.py 从 %s 生成，跳过前 %d 字节的图像头。" % (source, skip),
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
    print("%s: %dx%d, %d 段, 数据 %d 字节（原始 RGB888 %d 字节）"
          % (target, width, height, len(runs), len(runs) * 4, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
