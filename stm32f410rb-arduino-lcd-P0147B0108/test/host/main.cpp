/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms、0x3A=0x05）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. 竖屏 GRAM 列偏移 34（172 在 240 列里居中）
 *  4. 无触摸：Screen_HasTouch() == false
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

/* 找到最后一次出现的某条命令。 */
static const SimCommandRecord *last_command(uint8_t command)
{
    const SimCommandRecord *found = nullptr;
    for (int i = 0; i < Sim_CommandCount(); i++) {
        if (Sim_Command(i)->command == command) found = Sim_Command(i);
    }
    return found;
}

/* ------------------------------------------------------------ 初始化时序 */

static void check_init_sequence(void)
{
    static const uint8_t expected[] = {0x11, 0xB2, 0x35, 0x36, 0x3A, 0xB7, 0xBB, 0xC0, 0xC2, 0xC3,
                                       0xC4, 0xC6, 0xD0, 0xD0, 0xD6, 0xE0, 0xE1, 0x21, 0x29, 0x36};
    static const uint8_t expected_length[] = {0, 5, 1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 2, 2, 1, 14, 14, 0, 0, 1};
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
    check(sequence_ok, "初始化命令序列与参考工程 lcd_init.c 一致");
    check(length_ok, "各命令的数据字节数与参考工程一致");

    check(Sim_Command(3)->data[0] == 0x00, "首条 0x36 = 0x00（USE_HORIZONTAL==0，竖屏）");
    check(Sim_Command(4)->data[0] == 0x05, "0x3A = 0x05（RGB565，16-bit/pixel）");

    const SimCommandRecord *sleep_out = Sim_Command(0);
    const SimCommandRecord *after = Sim_Command(1);
    check(sleep_out->command == 0x11 && (after->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", after->time_ms - sleep_out->time_ms);

    const SimCommandRecord *display_on = Sim_Command(18);
    const SimCommandRecord *rotation = Sim_Command(19);
    check(display_on->command == 0x29 && (rotation->time_ms - display_on->time_ms) >= 20,
          "0x29 之后有 20 ms 延时（实际 %u ms）", rotation->time_ms - display_on->time_ms);
}

/* ------------------------------------------------------------ 演示画面 */

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    /* 与参考公式一致：start = i*172/6, end = (i+1)*172/6 - 1 */
    static const int start_x[6] = {0, 28, 57, 86, 114, 143};
    static const int end_x[6] = {27, 56, 85, 113, 142, 171};

    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const int mid = (start_x[i] + end_x[i]) / 2;
        const uint16_t got = Sim_Pixel(mid, 160);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条 (x=%d) = 0x%04X，期望 0x%04X\n", i, mid, got, want[i]);
        }
        /* 分界点也要落在正确的那一条里 */
        if (Sim_Pixel(start_x[i], 10) != want[i] || Sim_Pixel(end_x[i], 300) != want[i]) {
            ok = false;
            std::printf("      第 %d 条分界 [%d..%d] 颜色不对\n", i, start_x[i], end_x[i]);
        }
    }
    check(ok, "6 条彩条颜色与分界 x 与参考工程 DrawColorBars() 一致");
    check(Sim_Pixel(0, 0) == RED && Sim_Pixel(171, 319) == MAGENTA, "彩条覆盖整屏（左上红、右下品红）");
}

static void check_grayscale(void)
{
    /* 172/24 = 7，余 4 补在最后一格。 */
    const uint16_t first = Sim_Pixel(3, 160);
    const uint16_t last = Sim_Pixel(171, 160);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);

    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 160) > Sim_Pixel(x + 1, 160)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（24 格）");

    /* 每格边界：第 1 格结尾 x=6，第 2 格开始 x=7。 */
    check(Sim_Pixel(6, 160) == Sim_Pixel(0, 160) && Sim_Pixel(7, 160) != Sim_Pixel(6, 160),
          "灰阶第 1/2 格分界在 x=7（172/24=7）");
    /* 最后一格含余数：x=161..171 同为最亮。 */
    check(Sim_Pixel(161, 160) == Sim_Pixel(171, 160), "灰阶余数 4 像素补在最后一格");
}

/* ------------------------------------------------------------ 主流程 */

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();

    std::printf("== 初始化 ==\n");
    check(Screen_Init(), "Screen_Init() 返回成功");
    /* 背光脚 D6/PB10 由固件驱动：初始化时配成输出并拉低，画面准备好之前保持关闭。 */
    check(Sim_BacklightLevel() == 0, "初始化期间背光关闭（实际 %d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 2, "初始化只碰 D6 两次（pinMode + 拉低，实际 %d）",
          Sim_BacklightWriteCount());
    check(Sim_PixelCount() == 0, "初始化期间未写像素（%d）", Sim_PixelCount());
    check_init_sequence();

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
    check(std::strstr(Serial.log(), "touch=none") != nullptr, "串口日志报告无触摸");
    check(Screen_HasTouch() == false, "Screen_HasTouch() == false（本模组无触摸）");

    Sim_ResetPixelCounter();
    DemoApp_Setup();
    check(Sim_BacklightLevel() == 255, "setup 后背光打开（%d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 3, "setup 只额外写一次 D6（实际 %d）", Sim_BacklightWriteCount());
    check(Sim_PixelCount() == SIM_WIDTH * SIM_HEIGHT, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());

    /* 竖屏 GRAM 列偏移：可见第 0 列写到 GRAM 第 34 列。 */
    const SimCommandRecord *cas = last_command(0x2A);
    const SimCommandRecord *ras = last_command(0x2B);
    check(cas && ras, "setup 清屏发出了 0x2A/0x2B 窗口命令");
    if (cas && ras) {
        const int x0 = (cas->data[0] << 8) | cas->data[1];
        const int x1 = (cas->data[2] << 8) | cas->data[3];
        const int y0 = (ras->data[0] << 8) | ras->data[1];
        const int y1 = (ras->data[2] << 8) | ras->data[3];
        check(x0 == 34 && x1 == 34 + SIM_WIDTH - 1, "方向 0 列地址加 34（0x2A = %d..%d）", x0, x1);
        check(y0 == 0 && y1 == SIM_HEIGHT - 1, "方向 0 行地址无偏移（0x2B = %d..%d）", y0, y1);
    }
    Sim_ResetPixelCounter();

    std::printf("== 彩条（起始状态）==\n");
    g_base = millis();
    run_until(2500);
    check(Sim_PixelCount() == SIM_WIDTH * SIM_HEIGHT, "彩条写满整屏（%d 像素）", Sim_PixelCount());
    check_bars();
    check(Sim_GramPixel(34, 160) == RED, "可见列 0 落在 GRAM 第 34 列（0x%04X）", Sim_GramPixel(34, 160));
    check(Sim_GramPixel(33, 160) == 0x0000, "GRAM 第 0..33 列未被写（居中留边）");
    dump("1-colorbars");
    Sim_ResetPixelCounter();

    std::printf("== 灰阶（3s 后切换）==\n");
    run_until(4000);
    check(Sim_PixelCount() == SIM_WIDTH * SIM_HEIGHT, "灰阶写满整屏（%d 像素）", Sim_PixelCount());
    check_grayscale();
    dump("2-grayscale");
    Sim_ResetPixelCounter();

    std::printf("== 再切回彩条（6s 后切换）==\n");
    run_until(7500);
    check_bars();
    dump("3-colorbars-again");

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
