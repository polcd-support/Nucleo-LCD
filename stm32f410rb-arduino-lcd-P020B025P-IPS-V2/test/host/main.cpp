/*
 * 主机端行为验证：
 *  1. 初始化时序（复位、命令序列、0x11 后的 120 ms、末尾 0x2C 的 20 ms）
 *  2. 演示状态机每个画面的实际像素（导出 PNG 供人工核对）
 *  3. setup 清屏写满整屏、彩条 6 条、灰阶 24 级单调递增
 *  4. 无触摸设备：Screen_HasTouch() 为 false；背光脚 D6/PB10 由固件驱动
 *     （初始化拉低、DemoApp_Setup() 后拉高，全程只碰 3 次）
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
    /* 19 条面板初始化命令（参考工程 LCD_Init），其后 Screen_Init 会再写一条 0x36 应用方向。 */
    static const uint8_t expected[] = {0x11, 0xB2, 0x35, 0x36, 0x3A, 0xB7, 0xBB, 0xC0, 0xC2, 0xC3,
                                       0xC6, 0xD0, 0xD0, 0xD6, 0xE0, 0xE1, 0x21, 0x29, 0x2C, 0x36};
    static const uint8_t expected_length[] = {0, 5, 1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 2, 1, 14, 14, 0, 0, 0, 1};
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
    check(madctl->data[0] == 0x00, "首条 0x36 = 0x00（竖屏）");
    const SimCommandRecord *pixfmt = Sim_Command(4);
    check(pixfmt->data[0] == 0x05, "0x3A = 0x05（RGB565，16-bit/pixel）");

    const SimCommandRecord *sleep_out = Sim_Command(0);
    const SimCommandRecord *porch = Sim_Command(1);
    check(sleep_out->command == 0x11 && (porch->time_ms - sleep_out->time_ms) >= 120,
          "0x11 之后有 120 ms 延时（实际 %u ms）", porch->time_ms - sleep_out->time_ms);

    const SimCommandRecord *last_init = Sim_Command(18);
    const SimCommandRecord *rotation = Sim_Command(19);
    check(last_init->command == 0x2C && (rotation->time_ms - last_init->time_ms) >= 20,
          "0x2C 之后有 20 ms 延时（实际 %u ms）", rotation->time_ms - last_init->time_ms);

    std::printf("[info] 串口日志: %s\n", Serial.log());
    check(std::strstr(Serial.log(), "bpp=2") != nullptr, "面板像素格式为 RGB565（bpp=2）");
    check(std::strstr(Serial.log(), "touch=none") != nullptr, "设备报告无触摸");
}

/* ------------------------------------------------------------ 演示画面 */

static void check_bars(void)
{
    static const uint16_t want[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    bool ok = true;
    for (int i = 0; i < 6; i++) {
        const uint16_t got = Sim_Pixel(i * 40 + 20, 160);
        if (got != want[i]) {
            ok = false;
            std::printf("      第 %d 条（x=%d）= 0x%04X，期望 0x%04X\n", i, i * 40 + 20, got, want[i]);
        }
    }
    check(ok, "6 条彩条颜色与参考工程一致（x=20/60/100/140/180/220）");

    /* 相邻彩条的分界：39/40、79/80、119/120、159/160、199/200。 */
    check(Sim_Pixel(39, 160) == RED && Sim_Pixel(40, 160) == GREEN, "第 0/1 条分界在 x=39/40");
    check(Sim_Pixel(199, 160) == CYAN && Sim_Pixel(200, 160) == MAGENTA, "第 4/5 条分界在 x=199/200");
    check(Sim_Pixel(0, 0) != BLACK && Sim_Pixel(0, SIM_HEIGHT - 1) != BLACK, "彩条占满整屏高度");
}

static void check_grayscale(void)
{
    const uint16_t first = Sim_Pixel(3, 160);
    const uint16_t last = Sim_Pixel(SIM_WIDTH - 3, 160);
    check(first == 0x0000, "灰阶首格为黑（0x%04X）", first);
    check(near_color(last, 0xFFFF, 1), "灰阶末格为白（0x%04X）", last);
    bool monotonic = true;
    for (int x = 0; x < SIM_WIDTH - 1; x++) {
        if (Sim_Pixel(x, 160) > Sim_Pixel(x + 1, 160)) monotonic = false;
    }
    check(monotonic, "灰阶逐格递增（%d 格）", 24);
    check(Sim_Pixel(125, 0) == Sim_Pixel(125, SIM_HEIGHT - 1) && Sim_Pixel(125, 0) != 0x0000,
          "灰阶占满整屏高度（x=125 上下同色且非黑）");
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Sim_ResetPixelCounter();

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
    check(Sim_PixelCount() == SIM_WIDTH * SIM_HEIGHT, "setup 清屏写了整屏像素（%d）", Sim_PixelCount());
    check(!Screen_HasTouch(), "本设备无触摸（Screen_HasTouch() == false）");
    Sim_ResetPixelCounter();

    g_base = millis();

    std::printf("== 彩条 ==\n");
    run_until(1000);
    check(Sim_PixelCount() == SIM_WIDTH * SIM_HEIGHT, "彩条画满整屏 %d 像素（实际 %d）",
          SIM_WIDTH * SIM_HEIGHT, Sim_PixelCount());
    check_bars();
    dump("1-colorbar");

    std::printf("== 灰阶 ==\n");
    run_until(4000);
    check_grayscale();
    dump("2-grayscale");

    std::printf("== 第二轮回彩条 ==\n");
    run_until(7000);
    check_bars();
    dump("3-colorbar-2");

    check(Sim_BacklightWriteCount() == 3, "整个演示期间没有多余的背光操作（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
