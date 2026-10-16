#!/usr/bin/env python3
"""把 Image2Lcd 风格的 C 数组转成本工程使用的 RGB565 行程编码资产。

用法:
    python tools/pic2rle.py <输入头文件> <宽> <高> <输出头文件> [--skip 8] [--name kLogoImage] [--bpp 3]

输入文件是参考工程 HARDWARE/LCD/pic.h 那种形式：
    const unsigned char gImage_logo[185648] = { 0x10,0x12,... };

Image2Lcd 生成的数组前若干字节是图像头（含宽高），--skip 指定要跳过的字节数（默认 8）。
输入头文件里可能有多张图（本屏 pic.h 有 4 张），工具按“长度 = skip + 宽*高*bpp”自动选中那一张。
--bpp 是图像本体的字节数：3 = RGB888（默认，每像素 3 字节），2 = RGB565（每像素 2 字节，直接当作 uint16 大端）。
输出为 gfx/image.h 定义的 ScreenImage 结构，每两个 uint16 一组 {像素数, RGB565 颜色}。
"""
import re
import sys


def parse_arrays(path):
    """返回 {数组名: 字节串}。pic.h 里可能同时含多张图（本屏就是 4 张）。"""
    text = open(path, "rb").read()
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = text.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    arrays = {}
    for match in re.finditer(r"const\s+[\w\s]*?(\w+)\s*\[\s*\d+\s*\]\s*=\s*\{([^}]*)\}", text):
        arrays[match.group(1)] = bytes(int(value, 16) for value in re.findall(r"0[xX]([0-9a-fA-F]+)", match.group(2)))
    return arrays


def parse_array(path, expected_size):
    """取尺寸（含图像头）与期望一致的数组；只有一个且未指定尺寸时直接返回它。"""
    arrays = parse_arrays(path)
    if expected_size is not None:
        matches = [(name, data) for name, data in arrays.items() if len(data) == expected_size]
        if len(matches) == 1:
            return matches[0][1]
        if len(matches) > 1:
            raise SystemExit("多个数组长度都是 %d：%s" % (expected_size, ", ".join(n for n, _ in matches)))
        raise SystemExit("没有长度 %d 的数组，文件里的数组：%s"
                         % (expected_size, ", ".join("%s[%d]" % (n, len(d)) for n, d in arrays.items())))
    if len(arrays) == 1:
        return next(iter(arrays.values()))
    raise SystemExit("文件里有多个数组，无法自动选择：%s" % ", ".join(arrays))


def to_rgb565(data):
    words = []
    for i in range(0, len(data), 3):
        r, g, b = data[i], data[i + 1], data[i + 2]
        words.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return words


def to_rgb565_from_565(data):
    """RGB565 图像本体：每像素 2 字节大端，直接取为 uint16。"""
    words = []
    for i in range(0, len(data), 2):
        words.append((data[i] << 8) | data[i + 1])
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
    bpp = 3
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    if "--name" in sys.argv:
        name = sys.argv[sys.argv.index("--name") + 1]
    if "--bpp" in sys.argv:
        bpp = int(sys.argv[sys.argv.index("--bpp") + 1])
    if bpp not in (2, 3):
        print("--bpp 只能是 2（RGB565）或 3（RGB888）")
        return 1

    data = parse_array(source, skip + width * height * bpp)[skip:]
    if len(data) != width * height * bpp:
        print("像素数据长度 %d 与 %dx%d 不符（期望 %d）" % (len(data), width, height, width * height * bpp))
        return 1

    words = to_rgb565_from_565(data) if bpp == 2 else to_rgb565(data)
    runs = rle(words)
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
    print("%s: %dx%d, %d 段, 数据 %d 字节（原始图像 %d 字节，%d 字节/像素）"
          % (target, width, height, len(runs), len(runs) * 4, len(data), bpp))
    return 0


if __name__ == "__main__":
    sys.exit(main())
