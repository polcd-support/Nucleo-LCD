#!/usr/bin/env python3
"""把 Image2Lcd 风格的 C 数组转成本工程使用的 RGB565 行程编码资产。

用法:
    python tools/pic2rle.py <输入头文件> <宽> <高> <输出头文件>
                            [--skip 8] [--bpp 3] [--name kLogoImage]

输入文件是参考工程 HARDWARE/LCD/pic.h 那种形式：
    const unsigned char gImage_logo[105608] = { 0x10,0x12,... };

--bpp 指定每个像素的原始字节数：
    3 = RGB888（默认，保持向后兼容），2 = RGB565（本屏 gImage_logo 就是这种）。
Image2Lcd 生成的数组前若干字节是图像头（含宽高），--skip 指定要跳过的字节数（默认 8）。
一个头文件里有多个数组时，按“数据长度 == 宽*高*bpp”自动选中唯一匹配的那个。
输出为 gfx/image.h 定义的 ScreenImage 结构，每两个 uint16 一组 {像素数, RGB565 颜色}。
"""
import re
import sys


def parse_arrays(path):
    """返回 [(数组名, bytes), ...]，按出现顺序。"""
    text = open(path, "rb").read()
    for encoding in ("utf-8", "gbk", "latin-1"):
        try:
            text = text.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    arrays = []
    for match in re.finditer(r"const\s+[\w\s]+?\s(\w+)\s*\[\s*\d*\s*\]\s*=\s*\{([^}]*)\}", text):
        arrays.append((match.group(1), bytes(int(value, 16) for value in re.findall(r"0[xX]([0-9a-fA-F]+)", match.group(2)))))
    if not arrays:
        raise SystemExit("未在 %s 中找到图像数组" % path)
    return arrays


def to_rgb565(data, bpp):
    words = []
    if bpp == 2:
        for i in range(0, len(data), 2):
            words.append((data[i] << 8) | data[i + 1])
        return words
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
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    if "--bpp" in sys.argv:
        bpp = int(sys.argv[sys.argv.index("--bpp") + 1])
    if "--name" in sys.argv:
        name = sys.argv[sys.argv.index("--name") + 1]

    expected = width * height * bpp
    matches = []
    for array_name, raw in parse_arrays(source):
        data = raw[skip:]
        if len(data) == expected:
            matches.append((array_name, data))
    if len(matches) != 1:
        print("%s 中匹配 %dx%d bpp=%d（%d 字节）的数组有 %d 个：%s"
              % (source, width, height, bpp, expected, len(matches), [m[0] for m in matches]))
        return 1
    array_name, data = matches[0]

    runs = rle(to_rgb565(data, bpp))
    runs_name = "k" + name[1:] + "Runs"
    lines = [
        "/*",
        " * %dx%d RGB565 行程编码资产（见 ../gfx/image.h）。" % (width, height),
        " *",
        " * 由 tools/pic2rle.py 从 %s 的 %s 生成，跳过前 %d 字节的图像头，每像素 %d 字节。" % (source, array_name, skip, bpp),
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
    print("%s: %s %dx%d, %d 段, 数据 %d 字节（原始 %d 字节）"
          % (target, array_name, width, height, len(runs), len(runs) * 4, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
