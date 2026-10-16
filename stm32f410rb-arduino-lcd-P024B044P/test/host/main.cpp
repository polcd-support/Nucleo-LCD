/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. RLE LOGO 的像素数与参考工程一致
 *  4. 手写笔画、Clear 按钮、Dial 按钮与拨号画面
 *  5. 触摸坐标在 4 个方向下的换算
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
    const SimCommandRecord *next = Sim_Command(1);
    check(sleep_out->command == 0x11 && (next->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", next->time_ms - sleep_out->time_ms);

    const SimCommandRecord *inversion = Sim_Command(13);
    const SimCommandRecord *display_on = Sim_Command(14);
    check(inversion->command == 0x21 && display_on->command == 0x29,
          "0x21（Display Inversion On）在 0x29 之前");

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "chip=0x64") != nullptr, "触摸芯片 ID 读回 0x64");
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_logo(void)
{
    const uint16_t corner = Sim_Pixel(0, 29);
    const uint16_t last = Sim_Pixel(239, 248);
    check(corner == WHITE, "LOGO 左上角 (0,29) 为白色（0x%04X）", corner);
    check(last == WHITE, "LOGO 右下角 (239,248) 为白色（0x%04X）", last);
    check(Sim_Pixel(0, 28) == BLACK, "LOGO 上边（第 28 行）仍是黑底");
    check(Sim_Pixel(0, 249) == BLACK, "LOGO 下边（第 249 行）仍是黑底");
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
    const uint16_t center = Sim_Pixel(120, 160);
    const uint16_t corner = Sim_Pixel(2, 2);
    check(center == want && corner == want, "%s 全屏填充正确（中心 0x%04X 角 0x%04X）", name, center, corner);
}

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const uint16_t got = Sim_Pixel(i * 40 + 20, 160);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条 = 0x%04X，期望 0x%04X\n", i, got, want[i]);
        }
    }
    check(ok, "6 条彩条（每条约 40 像素）颜色与参考工程一致");
    check(Sim_Pixel(0, 0) == RED && Sim_Pixel(239, 319) == MAGENTA, "彩条满高（顶部红、底部洋红）");
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 160);
    const uint16_t last = Sim_Pixel(236, 160);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);
    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 160) > Sim_Pixel(x + 1, 160)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（24 格）");
}

static void check_handwriting(void)
{
    int stroke = 0;
    for (int y = 205; y < 255; y++)
        for (int x = 55; x < 70; x++)
            if (Sim_Pixel(x, y) == WHITE) stroke++;
    check(stroke > 40, "手写笔画已画出（白色像素 %d）", stroke);

    const uint16_t clear_button = Sim_Pixel(235, 315);
    check(clear_button == GRAY, "右下角 Clear 按钮为灰色（0x%04X）", clear_button);

    const uint16_t dial_button = Sim_Pixel(60, 315);
    check(dial_button == DARKBLUE, "左下角 Dial 按钮为深蓝色（0x%04X）", dial_button);

    int in_button = 0;
    for (int y = 290; y < 320; y++)
        for (int x = 180; x < 240; x++)
            if (Sim_Pixel(x, y) == GRAY) in_button++;
    check(in_button > 1000, "Clear 按钮区域已填充（%d 像素）", in_button);
}

static void check_cleared(void)
{
    int stroke = 0;
    for (int y = 205; y < 255; y++)
        for (int x = 55; x < 70; x++)
            if (Sim_Pixel(x, y) != BLACK) stroke++;
    check(stroke == 0, "点 Clear 后笔画被清除（残留 %d 像素）", stroke);
    check(Sim_Pixel(235, 315) == GRAY, "清屏后 Clear 按钮重新画出");
}

static void check_dialer(void)
{
    int digit = 0;
    for (int y = 44; y < 68; y++)
        for (int x = 14; x < 38; x++)
            if (Sim_Pixel(x, y) == WHITE) digit++;
    check(digit > 10, "号码框画出点下的数字（白色像素 %d）", digit);

    check(Sim_Pixel(95, 130) == LGRAYBLUE, "数字键为浅灰蓝色（0x%04X）", Sim_Pixel(95, 130));
    check(Sim_Pixel(20, 290) == GRAY, "Back 键为灰色");
    check(Sim_Pixel(100, 290) == GREEN, "Call 键为绿色");
    check(Sim_Pixel(172, 290) == BRRED, "Del 键为棕红色");
    check(Sim_Pixel(8, 60) == GRAY, "号码框左侧边框为灰色");

    /* 触摸调试行按参考工程 LCD_ShowIntNum() 高位补空格：点 (90,130) 时 X 字段应为 " 90"。 */
    const uint16_t x0 = 22;   /* 第 1 个字符格（应为空格） */
    const uint16_t x1 = 30;   /* 第 2 个字符格（'9'） */
    const uint16_t x2 = 38;   /* 第 3 个字符格（'0'） */
    int blank = 0, d9 = 0, d0 = 0;
    for (int y = 4; y < 20; y++) {
        for (int x = x0; x < x0 + 8; x++)
            if (Sim_Pixel(x, y) == WHITE) blank++;
        for (int x = x1; x < x1 + 8; x++)
            if (Sim_Pixel(x, y) == WHITE) d9++;
        for (int x = x2; x < x2 + 8; x++)
            if (Sim_Pixel(x, y) == WHITE) d0++;
    }
    check(blank == 0, "调试行 X 字段首位补空格、无笔画（白色像素 %d）", blank);
    check(d9 > 0 && d0 > 0, "调试行 X 字段后两位为数字（%d, %d）", d9, d0);
}

/* ------------------------------------------------------------ 触摸换算 */

static void check_touch_mapping(void)
{
    Screen_SetRotation(0);
    check(Screen_Width() == 240 && Screen_Height() == 320, "方向 0：240x320（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    Wire.deviceSetTouch(0x38, 1, 100, 200);
    uint16_t x = 0, y = 0;
    uint8_t count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 100 && y == 200, "方向 0：(100,200) 原样输出（实际 %u,%u, %u 点）", x, y, count);

    Screen_SetRotation(1);
    check(Screen_Width() == 240 && Screen_Height() == 320, "方向 1：240x320");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 139 && y == 119, "方向 1：镜像为 (139,119)（实际 %u,%u）", x, y);

    Screen_SetRotation(2);
    check(Screen_Width() == 320 && Screen_Height() == 240, "方向 2：320x240（实际 %ux%u）", Screen_Width(),
          Screen_Height());
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 200 && y == 100, "方向 2：交换为 (200,100)（实际 %u,%u）", x, y);

    Screen_SetRotation(3);
    check(Screen_Width() == 320 && Screen_Height() == 240, "方向 3：320x240");
    count = Screen_TouchRead(&x, &y);
    check(count == 1 && x == 119 && y == 139, "方向 3：(119,139)（实际 %u,%u）", x, y);

    Screen_SetRotation(0);
    Wire.deviceSetTouch(0x38, 0, 0, 0);
    count = Screen_TouchRead(&x, &y);
    check(count == 0, "抬手时返回 0 个触点");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Wire.deviceSet(0x38, 0xA3, 0x64); /* FT6236 芯片 ID */
    Wire.deviceSetTouch(0x38, 0, 0, 0);

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
    check(Sim_PixelCount() == 240 * 320, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());
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
    Wire.deviceSetTouch(0x38, 1, 60, 250);
    run_until(16520);
    Wire.deviceSetTouch(0x38, 1, 60, 230);
    run_until(16540);
    Wire.deviceSetTouch(0x38, 1, 60, 210);
    run_until(16560);
    check_handwriting();
    dump("6-handwriting");

    std::printf("== 清屏 ==\n");
    Wire.deviceSetTouch(0x38, 0, 0, 0);
    run_until(16600);
    Wire.deviceSetTouch(0x38, 1, 200, 300);
    run_until(16680);
    check_cleared();
    dump("7-cleared");

    std::printf("== 拨号 ==\n");
    Wire.deviceSetTouch(0x38, 0, 0, 0);
    run_until(16700);
    Wire.deviceSetTouch(0x38, 1, 35, 300);   /* Dial 按钮 -> 进入拨号画面 */
    run_until(16720);
    Wire.deviceSetTouch(0x38, 0, 0, 0);      /* 抬手解锁 */
    run_until(16760);
    Wire.deviceSetTouch(0x38, 1, 90, 130);   /* 数字键 "5"（x=90 -> 调试行 " 90"） */
    run_until(16780);
    Wire.deviceSetTouch(0x38, 0, 0, 0);
    run_until(16800);
    check_dialer();
    dump("8-dialer");

    std::printf("== 触摸换算 ==\n");
    check_touch_mapping();
    dump("9-rotation2");

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
