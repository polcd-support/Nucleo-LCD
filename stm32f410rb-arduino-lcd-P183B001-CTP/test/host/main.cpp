/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. RLE LOGO 的像素数与参考工程一致
 *  4. 手写笔画与 Clear 按钮（按钮只画不响应）
 *  5. 触摸坐标在 4 个方向下的换算（含 offset_y = -10 的钳位）
 *  6. 背光由固件驱动 D6/PB10（初始化拉低，DemoApp_Setup() 后拉高）
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

static void check_init_sequence(void)
{
    static const uint8_t expected[] = {0x11, 0x36, 0x3A, 0xB2, 0xB7, 0xBB, 0xC2, 0xC3,
                                       0xC4, 0xC6, 0xD0, 0xE0, 0xE1, 0x21, 0x29, 0x36};
    static const uint8_t expected_length[] = {0, 1, 1, 5, 1, 1, 1, 1,
                                             1, 1, 2, 14, 14, 0, 0, 1};
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

    const SimCommandRecord *madctl = Sim_Command(1);
    check(madctl->data[0] == 0x00, "首条 0x36 = 0x00（竖屏）");
    const SimCommandRecord *pixfmt = Sim_Command(2);
    check(pixfmt->data[0] == 0x05, "0x3A = 0x05（RGB565）");

    const SimCommandRecord *sleep_out = Sim_Command(0);
    const SimCommandRecord *after_sleep = Sim_Command(1);
    check(sleep_out->command == 0x11 && (after_sleep->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", after_sleep->time_ms - sleep_out->time_ms);

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "chip=0xB5") != nullptr, "触摸芯片 ID 读回 0xB5");
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_logo(void)
{
    check(Sim_Pixel(0, 29) == WHITE, "LOGO 左上角在 (0,29) 且为白色（0x%04X）", Sim_Pixel(0, 29));
    check(Sim_Pixel(0, 28) == BLACK, "LOGO 上方一行 (0,28) 已清成黑色");
    check(Sim_Pixel(239, 248) == WHITE, "LOGO 右下角（第 240x220 像素）在 (239,248)（0x%04X）",
          Sim_Pixel(239, 248));
    check(Sim_Pixel(239, 249) == BLACK, "LOGO 下方一行 (239,249) 仍是黑色");
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
    check(white > 50, "24 号白字有笔画像素（%d）", white);
    check(red > 200, "32 号中文红字有笔画像素（%d）", red);
    check(Sim_Pixel(0, 0) == BLACK, "文字页背景为黑");
}

static void check_solid(uint16_t want, const char *name)
{
    const uint16_t center = Sim_Pixel(120, 142);
    const uint16_t corner = Sim_Pixel(2, 2);
    check(center == want && corner == want, "%s 全屏填充正确（中心 0x%04X 角 0x%04X）", name, center, corner);
}

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const uint16_t got = Sim_Pixel(i * 40 + 20, 142);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条 = 0x%04X，期望 0x%04X\n", i, got, want[i]);
        }
    }
    check(ok, "6 条彩条（每条 40 px）颜色与参考工程一致");
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 142);
    const uint16_t last = Sim_Pixel(235, 142);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);
    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 142) > Sim_Pixel(x + 1, 142)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（24 格，余数 %d）", 240 % 24);
}

static void check_handwriting(void)
{
    int stroke = 0;
    for (int y = 205; y < 256; y++)
        for (int x = 50; x < 70; x++)
            if (Sim_Pixel(x, y) == WHITE) stroke++;
    check(stroke > 40, "手写笔画已画出（白色像素 %d）", stroke);

    const uint16_t button = Sim_Pixel(170, 275);
    check(button == GRAY, "右下角 Clear 按钮为灰色（0x%04X）", button);

    int in_button = 0;
    for (int y = 270; y < 284; y++)
        for (int x = 162; x < 240; x++)
            if (Sim_Pixel(x, y) == GRAY) in_button++;
    check(in_button > 800, "按钮区域已填充（%d 像素）", in_button);
}

static void check_button_noop(void)
{
    check(Sim_Pixel(60, 230) == WHITE, "点 Clear 按钮后笔画仍在（按钮不响应，与参考工程一致）");
    check(Sim_Pixel(170, 275) == GRAY, "按钮仍在");
}

/* ------------------------------------------------------------ 触摸换算 */

static void check_touch_mapping(void)
{
    Screen_SetRotation(0);
    check(Screen_Width() == 240 && Screen_Height() == 284, "方向 0：240x284（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    Wire.deviceSetTouch(0x15, 1, 100, 200);
    uint16_t x = 0, y = 0;
    uint8_t count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 100 && y == 73, "方向 0：(100,200) 经 Y 翻转及 offset_y=-10 变 (100,73)（实际 %u,%u, %u 点）",
          x, y, count);

    Wire.deviceSetTouch(0x15, 1, 100, 280);
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 100 && y == 0, "方向 0：raw y=280 经翻转和 -10 偏移被钳到 0（实际 %u,%u）", x, y);

    Wire.deviceSetTouch(0x15, 1, 100, 200);
    Screen_SetRotation(1);
    check(Screen_Width() == 240 && Screen_Height() == 284, "方向 1：240x284");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 139 && y == 210, "方向 1：镜像为 (139,210)（实际 %u,%u）", x, y);

    Screen_SetRotation(2);
    check(Screen_Width() == 284 && Screen_Height() == 240, "方向 2：284x240（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 73 && y == 100, "方向 2：交换为 (73,100)（实际 %u,%u）", x, y);

    Screen_SetRotation(3);
    check(Screen_Width() == 284 && Screen_Height() == 240, "方向 3：284x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 210 && y == 139, "方向 3：(210,139)（实际 %u,%u）", x, y);

    Screen_SetRotation(0);
    Wire.deviceSetTouch(0x15, 0, 0, 0);
    count = Screen_TouchRead(&x, &y);
    check(count == 0, "抬手时返回 0 个触点");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Wire.deviceSet(0x15, 0xA7, 0xB5); /* CST816 芯片 ID */
    Wire.deviceSetTouch(0x15, 0, 0, 0);

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
    check(Sim_PixelCount() == 240 * 284, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());
    Sim_ResetPixelCounter();

    g_base = millis();

    std::printf("== LOGO ==\n");
    run_until(2500);
    check(Sim_PixelCount() == 240 * 220, "RLE LOGO 展开 %d 像素（期望 %d）", Sim_PixelCount(), 240 * 220);
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
    Wire.deviceSetTouch(0x15, 1, 60, 23);
    run_until(16520);
    Wire.deviceSetTouch(0x15, 1, 60, 43);
    run_until(16540);
    Wire.deviceSetTouch(0x15, 1, 60, 63);
    run_until(16560);
    check_handwriting();
    dump("6-handwriting");

    Wire.deviceSetTouch(0x15, 0, 0, 0);
    run_until(16600);
    Wire.deviceSetTouch(0x15, 1, 200, 23); /* Y 翻转后落在 80x50 按钮内，但不响应 */
    run_until(16680);
    check_button_noop();
    dump("7-button");

    std::printf("== 触摸换算 ==\n");
    check_touch_mapping();
    dump("8-rotation2");

    Wire.deviceSetTouch(0x15, 0, 0, 0);
    run_until(16800);
    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
