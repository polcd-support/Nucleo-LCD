/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. RLE LOGO 的像素数与参考工程一致
 *  4. 手写笔画（参考工程进入手写画面不画 Clear 按钮）
 *  5. 触摸坐标在 4 个方向下的换算与抬手判定
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

static bool near_color(uint16_t got, uint16_t want, int tolerance)
{
    const int r1 = (got >> 11) & 0x1F, g1 = (got >> 5) & 0x3F, b1 = got & 0x1F;
    const int r2 = (want >> 11) & 0x1F, g2 = (want >> 5) & 0x3F, b2 = want & 0x1F;
    return (r1 - r2 <= tolerance && r2 - r1 <= tolerance) &&
           (g1 - g2 <= tolerance && g2 - g1 <= tolerance) &&
           (b1 - b2 <= tolerance && b2 - b1 <= tolerance);
}

/* ------------------------------------------------------------ 初始化时序 */

/* 参考工程 LCD_Init() 的逐条命令（USE_HORIZONTAL=0）。末尾多一条由 Screen_Init()
 * 写默认方向 MADCTL 产生的 0x36。 */
static const uint8_t expected[] = {
    0xEF, 0xEB, 0xFE, 0xEF, 0xEB, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0xB6, 0x36, 0x3A,
    0x90, 0xBD, 0xBC, 0xFF, 0xC3, 0xC4, 0xC9, 0xBE, 0xE1, 0xDF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xED, 0xAE, 0xCD, 0x70, 0xE8, 0x62,
    0x63, 0x64, 0x66, 0x67, 0x74, 0x98, 0x35, 0x21, 0x11, 0x29,
    0x36,
};
static const uint8_t expected_length[] = {
    0, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 1, 1,
    4, 1, 1, 3, 1, 1, 1, 1, 2, 3,
    6, 6, 6, 6, 2, 1, 1, 9, 1, 12,
    12, 7, 10, 10, 7, 2, 0, 0, 0, 0,
    1,
};

static void check_init_sequence(void)
{
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
    check(sequence_ok, "初始化命令序列与面板表一致");
    check(length_ok, "各命令的数据字节数与面板表一致");

    const SimCommandRecord *madctl = Sim_Command(18);
    check(madctl->data[0] == 0x08, "首条 0x36 = 0x08（竖屏）");
    const SimCommandRecord *pixfmt = Sim_Command(19);
    check(pixfmt->data[0] == 0x05, "0x3A = 0x05（RGB565，16-bit/pixel）");

    const SimCommandRecord *sleep_out = Sim_Command(48);
    const SimCommandRecord *display_on = Sim_Command(49);
    check(sleep_out->command == 0x11 && (display_on->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", display_on->time_ms - sleep_out->time_ms);

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "chip=0xB5") != nullptr, "触摸芯片 ID 读回 0xB5");
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
    check(std::strstr(Serial.log(), "P128H002-IPS-CTP") != nullptr, "面板名正确");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_logo(void)
{
    check(Sim_Pixel(29, 29) == WHITE, "LOGO 左上角 (29,29) 为白色（0x%04X）", Sim_Pixel(29, 29));
    check(Sim_Pixel(228, 211) == WHITE, "LOGO 右下角 (228,211) 已画出（0x%04X）", Sim_Pixel(228, 211));
    check(Sim_Pixel(28, 29) == BLACK && Sim_Pixel(229, 211) == BLACK, "LOGO 之外的区域仍为黑");
    check(Sim_Pixel(0, 0) == BLACK && Sim_Pixel(239, 239) == BLACK, "屏幕四角为黑（LOGO 未越界）");
}

static void check_text(void)
{
    int white = 0;
    int red = 0;
    for (int y = 50; y < 74; y++)
        for (int x = 20; x < 152; x++)
            if (Sim_Pixel(x, y) == WHITE) white++;
    for (int y = 150; y < 182; y++)
        for (int x = 80; x < 208; x++)
            if (Sim_Pixel(x, y) == RED) red++;
    check(white > 50, "24 号白字有笔画像素（%d）", white);
    check(red > 200, "32 号中文红字有笔画像素（%d）", red);
    check(Sim_Pixel(0, 0) == BLACK, "文字页背景为黑");
}

static void check_solid(uint16_t want, const char *name)
{
    const uint16_t center = Sim_Pixel(120, 120);
    const uint16_t corner = Sim_Pixel(2, 2);
    check(center == want && corner == want, "%s 全屏填充正确（中心 0x%04X 角 0x%04X）", name, center, corner);
}

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const uint16_t got = Sim_Pixel(i * 40 + 20, 120);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条 = 0x%04X，期望 0x%04X\n", i, got, want[i]);
        }
    }
    check(ok, "6 条彩条颜色与参考工程一致");
    check(Sim_Pixel(239, 120) == MAGENTA, "最后一条彩条铺到右边缘（0x%04X）", Sim_Pixel(239, 120));
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 120);
    const uint16_t last = Sim_Pixel(239, 120);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);
    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 120) > Sim_Pixel(x + 1, 120)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（%d 格）", 24);
}

static void check_handwriting(void)
{
    int stroke = 0;
    for (int y = 55; y < 105; y++)
        for (int x = 50; x < 70; x++)
            if (Sim_Pixel(x, y) == WHITE) stroke++;
    check(stroke > 40, "手写笔画已画出（白色像素 %d）", stroke);

    /* 参考工程进入手写画面不调用 DrawClearButton()，右下角必须是黑的。 */
    int gray = 0;
    for (int y = 210; y < 240; y++)
        for (int x = 180; x < 240; x++)
            if (Sim_Pixel(x, y) == GRAY) gray++;
    check(gray == 0, "右下角没有 Clear 按钮（灰色像素 %d）", gray);
    check(Sim_Pixel(210, 225) == BLACK, "按钮位置为黑（0x%04X）", Sim_Pixel(210, 225));
}

/* ------------------------------------------------------------ 触摸换算 */

static const uint8_t CST816_ADDR = 0x15;

static void check_touch_mapping(void)
{
    check(Screen_HasTouch(), "该设备有触摸（CST816 @0x15）");

    Screen_SetRotation(0);
    check(Screen_Width() == 240 && Screen_Height() == 240, "方向 0：240x240（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    Wire.deviceSetTouch(CST816_ADDR, 1, 100, 200);
    uint16_t x = 0, y = 0;
    uint8_t count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 100 && y == 200, "方向 0：(100,200) 原样输出（实际 %u,%u, %u 点）", x, y, count);

    Screen_SetRotation(1);
    check(Screen_Width() == 240 && Screen_Height() == 240, "方向 1：240x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 139 && y == 39, "方向 1：镜像为 (139,39)（实际 %u,%u）", x, y);

    Screen_SetRotation(2);
    check(Screen_Width() == 240 && Screen_Height() == 240, "方向 2：240x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 200 && y == 100, "方向 2：交换为 (200,100)（实际 %u,%u）", x, y);

    Screen_SetRotation(3);
    check(Screen_Width() == 240 && Screen_Height() == 240, "方向 3：240x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 39 && y == 139, "方向 3：(39,139)（实际 %u,%u）", x, y);

    Screen_SetRotation(0);
    Wire.deviceSetTouch(CST816_ADDR, 0, 0, 0);
    count = Screen_TouchRead(&x, &y);
    check(count == 0, "抬手时返回 0 个触点");
    count = Screen_TouchReadRaw(&x, &y);
    check(count == 0, "Screen_TouchReadRaw 同样返回 0 个触点");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Wire.deviceSet(CST816_ADDR, 0xA7, 0xB5); /* CST816S 芯片 ID */
    Wire.deviceSetTouch(CST816_ADDR, 0, 0, 0);

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
    check(Sim_PixelCount() == 240 * 240, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());
    Sim_ResetPixelCounter();

    g_base = millis();

    std::printf("== LOGO ==\n");
    run_until(2500);
    check(Sim_PixelCount() == 200 * 183, "RLE LOGO 展开 %d 像素（期望 %d）", Sim_PixelCount(), 200 * 183);
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
    Wire.deviceSetTouch(CST816_ADDR, 1, 60, 100);
    run_until(16520);
    Wire.deviceSetTouch(CST816_ADDR, 1, 60, 80);
    run_until(16540);
    Wire.deviceSetTouch(CST816_ADDR, 1, 60, 60);
    run_until(16560);
    check_handwriting();
    dump("6-handwriting");

    Wire.deviceSetTouch(CST816_ADDR, 0, 0, 0);
    run_until(16600);
    Wire.deviceSetTouch(CST816_ADDR, 1, 150, 60);
    run_until(16700);
    check(Sim_Pixel(150, 60) == WHITE, "抬手后重按在新位置画出点（0x%04X）", Sim_Pixel(150, 60));
    check(Sim_Pixel(100, 60) == BLACK, "抬手复位后不会把旧位置连到新位置（连线已断开）");
    dump("7-handwriting2");

    std::printf("== 触摸换算 ==\n");
    check_touch_mapping();
    dump("8-rotation2");

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
