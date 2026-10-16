/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms）
 *  2. 演示序列每个画面的实际像素（导出 PNG 供人工核对）
 *  3. RLE LOGO 的像素数（240x220，位置 (0,10)）
 *  4. 无触摸设备：Screen_HasTouch() == false
 *  5. 序列循环一圈后回到 LOGO 画面
 *
 * 构建与运行：test/run.sh
 */
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "Arduino.h"
#include "SPI.h"
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
    static const uint8_t expected[] = {0x11, 0xB2, 0x35, 0x36, 0x3A, 0xB7, 0xBB, 0xC0, 0xC2,
                                       0xC3, 0xC4, 0xC6, 0xD6, 0xD0, 0xD0, 0xD6, 0xE0, 0xE1,
                                       0xE4, 0x21, 0x29, 0x2A, 0x2B, 0x2C, 0x36};
    static const uint8_t expected_length[] = {0, 5, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 2, 2, 1, 14, 14,
                                             3, 0, 0, 4, 4, 0, 1};
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

    const SimCommandRecord *madctl = Sim_Command(3);
    check(madctl->data[0] == 0x00, "面板表里的 0x36 = 0x00（竖屏正向）");
    const SimCommandRecord *pixfmt = Sim_Command(4);
    check(pixfmt->data[0] == 0x05, "0x3A = 0x05（RGB565）");
    const SimCommandRecord *last = Sim_Command(count - 1);
    check(last->command == 0x36 && last->data[0] == 0x00, "最后一条为方向 0 的 MADCTL 0x36=0x00");

    const SimCommandRecord *sleep_out = Sim_Command(0);
    const SimCommandRecord *porch = Sim_Command(1);
    check(sleep_out->command == 0x11 && (porch->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", porch->time_ms - sleep_out->time_ms);

    /* 参考工程在第一、第二条 0xD0 之间 delay_ms(10)。 */
    const SimCommandRecord *d0a = Sim_Command(13);
    const SimCommandRecord *d0b = Sim_Command(14);
    check(d0a->command == 0xD0 && d0b->command == 0xD0 && (d0b->time_ms - d0a->time_ms) >= 10,
          "第一条 0xD0 与第二条 0xD0 之间有 10 ms 延时（实际 %u ms）", d0b->time_ms - d0a->time_ms);

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "touch=none") != nullptr, "设备无触摸（touch=none）");
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
    check(std::strstr(Serial.log(), "240x240") != nullptr, "分辨率为 240x240");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_logo(void)
{
    const uint16_t top = Sim_Pixel(0, 10);
    const uint16_t last = Sim_Pixel(239, 229);
    check(top == WHITE, "LOGO 左上角 (0,10) 为白色（0x%04X）", top);
    check(last == WHITE, "LOGO 右下角 (239,229) 已画出（0x%04X）", last);
    check(Sim_Pixel(0, 9) == BLACK && Sim_Pixel(0, 230) == BLACK, "LOGO 上下各留黑边（y=9/230 为黑）");
}

static int count_color(int x0, int y0, int x1, int y1, uint16_t color)
{
    int total = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (Sim_Pixel(x, y) == color) total++;
    return total;
}

static void check_text(void)
{
    const int white = count_color(42, 52, 174, 76, WHITE);
    const int blue1 = count_color(56, 100, 143, 116, BLUE);
    const int blue2 = count_color(56, 124, 143, 140, BLUE);
    const int red = count_color(56, 168, 183, 200, RED);
    check(white > 50, "24 号白字有笔画像素（%d）", white);
    check(blue1 > 50, "16 号蓝字“240x240 IPS”有笔画像素（%d）", blue1);
    check(blue2 > 50, "16 号蓝字“ST7789P3 SPI”有笔画像素（%d）", blue2);
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
    check(ok, "6 条彩条颜色与参考工程一致（每条 40 像素宽）");
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 120);
    const uint16_t last = Sim_Pixel(235, 120);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);
    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 120) > Sim_Pixel(x + 1, 120)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（24 格）");
}

static void check_no_touch(void)
{
    check(!Screen_HasTouch(), "Screen_HasTouch() == false");
    uint16_t x = 0, y = 0;
    check(Screen_TouchRead(&x, &y) == 0, "无触摸时 Screen_TouchRead() 返回 0 点");
    check(Screen_TouchReadRaw(&x, &y) == 0, "无触摸时 Screen_TouchReadRaw() 返回 0 点");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();

    std::printf("== 初始化 ==\n");
    check(Screen_Init(), "Screen_Init() 返回成功");
    check(Screen_Width() == 240 && Screen_Height() == 240, "分辨率 240x240（实际 %ux%u）", Screen_Width(),
          Screen_Height());
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

    std::printf("== 循环第二圈 ==\n");
    /* 一整圈 = 3000+2000+5*1000+3000+3000 = 16000 ms，第二圈 LOGO 在 16~19 s 之间。 */
    run_until(15990);
    Sim_ResetPixelCounter();
    run_until(17000);
    check(Sim_PixelCount() == 240 * 240 + 240 * 220, "第二圈 LOGO 前清屏 + LOGO（%d 像素）", Sim_PixelCount());
    check_logo();
    dump("6-logo-loop");

    std::printf("== 无触摸 ==\n");
    check_no_touch();

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
