/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms、背光脚由固件在初始化时拉低）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. RLE LOGO 的像素数与位置
 *  4. 手写笔画与 Clear 按钮（按钮只画不响应）
 *  5. 触摸坐标在 4 个方向下的换算（含 CST816 的 5 像素 Y 偏移）
 *  6. GRAM 偏移在各方向下生效（0x2A/0x2B 的起始地址）
 *
 * 构建与运行：test/run.sh
 */
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "sim.h"

#include "../../src/app/demo_app.h"
#include "../../src/screen/gfx/gfx.h"
#include "../../src/screen/screen.h"

#define TOUCH_ADDR 0x15

static int g_failures = 0;
static uint32_t g_base = 0;
static const char *g_output_dir = "test/out";

static void check(bool ok, const char *format, ...)
{
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    std::printf("[%s] %s\n", ok ? "ok  " : "FAIL", message);
    if (!ok) g_failures++;
}

static void run_until(uint32_t relative_ms)
{
    const uint32_t target = g_base + relative_ms;
    while (millis() < target) {
        DemoApp_Loop();
        delay(1);
    }
}

static void dump(const char *name)
{
    char path[256];
    std::snprintf(path, sizeof(path), "%s/%s.png", g_output_dir, name);
    Sim_WritePng(path);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* ------------------------------------------------------------ 初始化时序 */

static void check_init_sequence(void)
{
    static const uint8_t expected[] = {0x11, 0x36, 0x3A, 0xB2, 0xB7, 0xBB, 0xC2, 0xC3, 0xC4,
                                       0xC6, 0xD0, 0xE0, 0xE1, 0x21, 0x29, 0x36};
    static const uint8_t expected_length[] = {0, 1, 1, 5, 1, 1, 1, 1, 1, 1, 2, 14, 14, 0, 0, 1};
    const int count = Sim_CommandCount();
    check(count == (int)sizeof(expected), "初始化共 %u 条命令（实际 %d）", (unsigned)sizeof(expected), count);

    bool sequence_ok = true;
    bool length_ok = true;
    const int limit = count < (int)sizeof(expected) ? count : (int)sizeof(expected);
    for (int i = 0; i < limit; i++) {
        const SimCommandRecord *record = Sim_Command(i);
        if (record->command != expected[i]) {
            sequence_ok = false;
            std::printf("      第 %d 条命令 = 0x%02X，期望 0x%02X\n", i, record->command, expected[i]);
        }
        if (record->length != expected_length[i]) {
            length_ok = false;
            std::printf("      0x%02X 数据长度 = %u，期望 %u\n", record->command, record->length,
                        expected_length[i]);
        }
    }
    check(sequence_ok, "初始化命令序列与面板表一致（lcd_init.c 逐条转写 + 末尾 MADCTL）");
    check(length_ok, "各命令的数据字节数与面板表一致");

    const SimCommandRecord *madctl = Sim_Command(1);
    check(madctl->data[0] == 0x00, "首条 0x36 = 0x00（竖屏）");
    const SimCommandRecord *pixfmt = Sim_Command(2);
    check(pixfmt->data[0] == 0x05, "0x3A = 0x05（RGB565，每像素 2 字节）");

    const SimCommandRecord *sleep_out = Sim_Command(0);
    const SimCommandRecord *next = Sim_Command(1);
    check(sleep_out->command == 0x11 && (next->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", next->time_ms - sleep_out->time_ms);

    check(Sim_FirstSpiMs() > 0, "面板 SPI 已有字节发出（首个字节 @%u ms）", Sim_FirstSpiMs());

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "chip=0xB5") != nullptr, "触摸芯片 ID 读回 0xB5（CST816）");
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
    check(std::strstr(Serial.log(), "240x280") != nullptr, "面板分辨率为 240x280");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_logo(void)
{
    check(Sim_PixelCount() == 240 * 220, "RLE LOGO 展开 %d 像素（期望 %d）", Sim_PixelCount(), 240 * 220);
    check(Sim_WindowX0() == 0 && Sim_WindowY0() == 29 + 20,
          "LOGO 窗口在 GRAM (0,%d)（参考工程 y+20 偏移）", 29 + 20);
    check(Sim_Pixel(0, 29) == WHITE, "LOGO 左上角为白色（0x%04X）", Sim_Pixel(0, 29));
    check(Sim_Pixel(239, 248) == WHITE, "LOGO 右下角（第 240x220 像素）已画出（0x%04X）", Sim_Pixel(239, 248));
    check(Sim_Pixel(0, 0) == BLACK && Sim_Pixel(239, 249) == BLACK, "LOGO 之外仍为黑");
    check(Sim_Rotation() == 0, "当前方向为 0（实际 %d）", Sim_Rotation());
}

static void check_text(void)
{
    int white = 0;
    int red = 0;
    for (int y = 50; y < 74; y++)
        for (int x = 20; x < 164; x++)
            if (Sim_Pixel(x, y) == WHITE) white++;
    for (int y = 150; y < 182; y++)
        for (int x = 80; x < 208; x++)
            if (Sim_Pixel(x, y) == RED) red++;
    int blue = 0;
    for (int y = 100; y < 116; y++)
        for (int x = 30; x < 156; x++)
            if (Sim_Pixel(x, y) == BLUE) blue++;
    check(white > 50, "24 号白字有笔画像素（%d）", white);
    check(blue > 50, "16 号蓝字有笔画像素（%d）", blue);
    check(red > 200, "32 号中文红字有笔画像素（%d）", red);
    check(Sim_Pixel(0, 0) == BLACK, "文字页背景为黑");
}

static void check_solid(uint16_t want, const char *name)
{
    const uint16_t center = Sim_Pixel(120, 140);
    const uint16_t corner = Sim_Pixel(2, 2);
    check(center == want && corner == want, "%s 全屏填充正确（中心 0x%04X 角 0x%04X）", name, center, corner);
}

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const uint16_t got = Sim_Pixel(i * 40 + 20, 140);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条 = 0x%04X，期望 0x%04X\n", i, got, want[i]);
        }
    }
    check(ok, "6 条彩条（每条 40 像素）颜色与参考工程一致");
    check(Sim_WindowY0() == 20, "彩条窗口行起始 = 20（方向 0 的 GRAM 偏移）");
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 140);
    const uint16_t last = Sim_Pixel(236, 140);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(last == 0xFFFF, "灰阶末格为白（0x%04X）", last);

    bool levels_ok = true;
    for (int n = 0; n < 24; n++) {
        const uint8_t gray = (uint8_t)((n * 255) / 23);
        const uint16_t want = rgb565(gray, gray, gray);
        const uint16_t got = Sim_Pixel(n * 10 + 5, 140);
        if (got != want) {
            levels_ok = false;
            std::printf("      第 %d 格 = 0x%04X，期望 0x%04X（gray=%u）\n", n, got, want, gray);
        }
    }
    check(levels_ok, "24 级灰阶每格颜色 = RGB565(n*255/23)");

    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 140) > Sim_Pixel(x + 1, 140)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（24 格，余数 %d 补在最后一格）", SIM_WIDTH % 24);
}

/* 手写区（触点在屏幕坐标 (60,165)..(60,205) 附近）内的白色像素数。 */
static int stroke_pixels(void)
{
    int stroke = 0;
    for (int y = 160; y < 212; y++)
        for (int x = 50; x < 70; x++)
            if (Sim_Pixel(x, y) == WHITE) stroke++;
    return stroke;
}

static int button_pixels(uint16_t color)
{
    int count = 0;
    for (int y = 250; y < 280; y++)
        for (int x = 180; x < 240; x++)
            if (Sim_Pixel(x, y) == color) count++;
    return count;
}

static void check_handwriting(void)
{
    const int stroke = stroke_pixels();
    check(stroke > 40, "手写笔画已画出（白色像素 %d）", stroke);

    check(Sim_Pixel(200, 260) == GRAY, "右下角 Clear 按钮为灰色（0x%04X）", Sim_Pixel(200, 260));
    check(button_pixels(GRAY) > 1000, "按钮区域已填充（%d 像素）", button_pixels(GRAY));
    check(button_pixels(BLACK) > 20, "按钮上的 16 号 \"Clear\" 黑字已画出（%d 像素）", button_pixels(BLACK));
}

/* 参考工程里按钮命中后的清屏被注释掉：点按钮不清屏，
 * 但触点本身仍会按手写逻辑画出点/线（与参考一致）。 */
static void check_button_ignored(void)
{
    check(stroke_pixels() > 40, "点 Clear 按钮后笔画仍在（白色像素 %d）", stroke_pixels());
    check(Sim_Pixel(200, 260) == GRAY, "按钮保持灰色，未被重画/清屏");
    check(Sim_Pixel(200, 265) == WHITE, "按钮上的触点仍按手写逻辑画点（与参考一致）");
    check(Sim_Pixel(130, 212) == BLACK, "到按钮的线段（跨度 > LCD_MAX_SEGMENT）被丢弃");
}

/* ------------------------------------------------------------ 触摸换算 */

static void check_touch_mapping(void)
{
    check(Screen_HasTouch(), "本设备有触摸（Screen_HasTouch()）");

    Screen_SetRotation(0);
    check(Screen_Width() == 240 && Screen_Height() == 280, "方向 0：240x280（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    Wire.deviceSetTouch(TOUCH_ADDR, 1, 100, 200);
    uint16_t raw_x = 0, raw_y = 0, x = 0, y = 0;
    const uint8_t raw_count = Screen_TouchReadRaw(&raw_x, &raw_y);
    check(raw_count == 1 && raw_x == 100 && raw_y == 200,
          "原生坐标原样读出 (100,200)（实际 %u,%u）", raw_x, raw_y);
    uint8_t count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 100 && y == 205, "方向 0：(100,205)（CST816 的 5 像素 Y 偏移生效，实际 %u,%u）", x, y);

    Screen_SetRotation(1);
    check(Screen_Width() == 240 && Screen_Height() == 280, "方向 1：240x280");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 139 && y == 74, "方向 1：镜像为 (139,74)（实际 %u,%u）", x, y);

    Screen_SetRotation(2);
    check(Screen_Width() == 280 && Screen_Height() == 240, "方向 2：280x240（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 205 && y == 100, "方向 2：交换为 (205,100)（实际 %u,%u）", x, y);

    Screen_SetRotation(3);
    check(Screen_Width() == 280 && Screen_Height() == 240, "方向 3：280x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 74 && y == 139, "方向 3：(74,139)（实际 %u,%u）", x, y);

    Screen_SetRotation(0);
    Wire.deviceSetTouch(TOUCH_ADDR, 0, 0, 0);
    count = Screen_TouchRead(&x, &y);
    check(count == 0, "抬手时返回 0 个触点");
}

/* ------------------------------------------------------------ GRAM 偏移 */

static void check_rotation_offsets(void)
{
    Screen_SetRotation(1);
    LCD_Fill(GREEN);
    check(Sim_WindowY0() == 80, "方向 1 行起始 = 80（参考工程 LCD_Address_Set 的 y+80）");
    check(Sim_Pixel(0, 0) == GREEN && Sim_Pixel(239, 279) == GREEN, "方向 1 全屏填充完整");

    Screen_SetRotation(2);
    LCD_Fill(RED);
    check(Sim_WindowX0() == 0 && Sim_WindowY0() == 0, "方向 2 无偏移（列 %d 行 %d）", Sim_WindowX0(),
          Sim_WindowY0());

    Screen_SetRotation(3);
    LCD_Fill(BLUE);
    check(Sim_WindowX0() == 80 && Sim_WindowY0() == 0, "方向 3 列起始 = 80（参考工程 LCD_Address_Set 的 x+80）");

    Screen_SetRotation(0);
    LCD_Fill(BLACK);
    check(Sim_WindowX0() == 0 && Sim_WindowY0() == 20, "方向 0 列起始 = 0、行起始 = 20");
    check(Sim_Pixel(0, 0) == BLACK && Sim_Pixel(239, 279) == BLACK, "方向 0 全屏填充完整（末尾 20 行未越界）");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Wire.deviceSet(TOUCH_ADDR, 0xA7, 0xB5); /* CST816S 芯片 ID */
    Wire.deviceSetTouch(TOUCH_ADDR, 0, 0, 0);

    std::printf("== 初始化 ==\n");
    check(Screen_Init(), "Screen_Init() 返回成功");
    /* 背光脚 D6/PB10 由固件驱动：初始化时配成输出并拉低，画面准备好之前保持关闭。 */
    check(Sim_BacklightLevel() == 0, "初始化期间背光关闭（实际 %d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 2, "初始化只碰 D6 两次（pinMode + 拉低，实际 %d）",
          Sim_BacklightWriteCount());
    check(Sim_PixelCount() == 0, "初始化期间未写像素（%d）", Sim_PixelCount());
    check_init_sequence();
    Sim_ResetPixelCounter();

    DemoApp_Setup();
    check(Sim_BacklightLevel() == 255, "setup 后背光打开（%d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 3, "setup 只额外写一次 D6（实际 %d）", Sim_BacklightWriteCount());
    check(Sim_PixelCount() == 240 * 280, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());
    check(Sim_WindowX0() == 0 && Sim_WindowY0() == 20, "清屏窗口从 GRAM (0,20) 开始（方向 0 偏移）");
    Sim_ResetPixelCounter();

    g_base = millis();

    std::printf("== LOGO ==\n");
    run_until(2500);
    check_logo();
    dump("1-logo");

    std::printf("== 文字 ==\n");
    run_until(4000);
    check_text();
    dump("2-text");

    std::printf("== 全屏纯色 ==\n");
    run_until(5500);
    check_solid(RED, "红");
    dump("3-red");
    run_until(6500);
    check_solid(GREEN, "绿");
    run_until(7500);
    check_solid(BLUE, "蓝");
    run_until(8500);
    check_solid(WHITE, "白");
    run_until(9500);
    check_solid(BLACK, "黑");

    std::printf("== 彩条 ==\n");
    run_until(11500);
    check_bars();
    dump("4-colorbar");

    std::printf("== 灰阶 ==\n");
    run_until(14500);
    check_grayscale();
    dump("5-grayscale");

    std::printf("== 手写 ==\n");
    run_until(16500);
    Wire.deviceSetTouch(TOUCH_ADDR, 1, 60, 200);
    run_until(16520);
    Wire.deviceSetTouch(TOUCH_ADDR, 1, 60, 180);
    run_until(16540);
    Wire.deviceSetTouch(TOUCH_ADDR, 1, 60, 160);
    run_until(16560);
    check_handwriting();
    dump("6-handwriting");

    std::printf("== 按钮（只画不响应） ==\n");
    Wire.deviceSetTouch(TOUCH_ADDR, 0, 0, 0);
    run_until(16600);
    Wire.deviceSetTouch(TOUCH_ADDR, 1, 200, 260);
    run_until(16680);
    check_button_ignored();
    dump("7-button-ignored");

    std::printf("== 触摸换算 ==\n");
    check_touch_mapping();

    std::printf("== GRAM 偏移 ==\n");
    check_rotation_offsets();
    dump("8-rotation0");

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
