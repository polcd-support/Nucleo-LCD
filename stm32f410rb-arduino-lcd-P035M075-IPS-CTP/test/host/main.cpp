/* LCD_DEMO 状态机、LOGO、颜色页、灰阶和触摸手写的主机端验证。 */
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "Arduino.h"
#include "Wire.h"
#include "sim.h"

#include "../../src/app/demo_app.h"
#include "../../src/screen/gfx/gfx.h"
#include "../../src/screen/screen.h"

static int g_failures = 0;

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

static void advance(uint32_t ms)
{
    delay(ms);
    DemoApp_Loop();
}

static void check_solid(uint16_t color, const char *name)
{
    check(Sim_Pixel(0, 0) == color && Sim_Pixel(160, 240) == color &&
              Sim_Pixel(319, 479) == color,
          "%s整屏颜色正确", name);
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Sim_Reset();
    Wire.deviceSet(0x38, 0xA3, 0x64);
    Wire.deviceSetTouch(0x38, 0, 0, 0);

    check(Screen_Init(), "Screen_Init() 返回成功");
    /* 背光脚 D6/PB10 由固件驱动：初始化时配成输出并拉低，画面准备好之前保持关闭。 */
    check(Sim_BacklightLevel() == 0, "初始化期间背光关闭（实际 %d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 2, "初始化只碰 D6 两次（pinMode + 拉低，实际 %d）",
          Sim_BacklightWriteCount());

    DemoApp_Setup();
    check(Sim_BacklightLevel() == 255, "setup 后背光打开（%d）", Sim_BacklightLevel());
    check(Sim_BacklightWriteCount() == 3, "setup 只额外写一次 D6（实际 %d）", Sim_BacklightWriteCount());
    check(Sim_CommandCount() >= 18, "面板初始化命令已发送");
    check(Sim_Command(6)->command == 0x3A && Sim_Command(6)->data[0] == 0x66,
          "ILI9488 使用 RGB666（0x3A=0x66）");
    check(std::strstr(Serial.log(), "chip=0x64") != nullptr, "FT6336U 芯片 ID 正确");
    check(std::strstr(Serial.log(), "spi=24MHz") != nullptr, "LCD SPI 已配置为 24 MHz 高速档");
    check(Wire.deviceGet(0x38, 0x80) == 40, "FT6336U 触摸阈值为 40");
    check(Wire.deviceGet(0x38, 0xA4) == 0, "FT6336U 使用轮询模式");
    check(Sim_PixelCount() == 320 * 480 + 260 * 238,
          "首次清屏并展开 260x238 LOGO（像素 %d）", Sim_PixelCount());
    check(Sim_Pixel(0, 0) == WHITE, "LOGO 左上角已画出");
    check(Sim_Pixel(300, 300) == BLACK && Sim_Pixel(319, 479) == BLACK,
          "LOGO 外区域为黑色");
    Sim_WritePng("test/out/1-logo.png");

    Sim_ResetPixelCounter();
    advance(2999);
    check(Sim_PixelCount() == 0, "LOGO 保持满 3 秒期间不重复刷新");
    advance(1);
    int white_pixels = 0;
    int red_pixels = 0;
    for (int y = 50; y < 182; y++) {
        for (int x = 20; x < 220; x++) {
            const uint16_t pixel = Sim_Pixel(x, y);
            if (pixel == WHITE) white_pixels++;
            if (pixel == RED) red_pixels++;
        }
    }
    check(white_pixels > 50 && red_pixels > 200, "文字页包含英文和红色中文");
    check(Sim_BacklightLevel() == 255, "文字页完成后背光打开");
    Sim_WritePng("test/out/2-text.png");

    advance(2000);
    check_solid(RED, "红");
    advance(1000);
    check_solid(GREEN, "绿");
    advance(1000);
    check_solid(BLUE, "蓝");
    advance(1000);
    check_solid(WHITE, "白");
    advance(1000);
    check_solid(BLACK, "黑");

    advance(1000);
    static const uint16_t bar_colors[8] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, BLACK, WHITE};
    bool bars_ok = true;
    for (int i = 0; i < 8; i++) {
        if (Sim_Pixel(i * 40 + 20, 240) != bar_colors[i]) bars_ok = false;
    }
    check(bars_ok, "8 条 40 px 彩条正确");
    check(Sim_BacklightLevel() == 255, "彩条完成后背光打开");
    Sim_WritePng("test/out/3-color-bars.png");

    advance(3000);
    check(Sim_Pixel(3, 240) == BLACK, "灰阶首格为黑色");
    check(Sim_Pixel(319, 240) == WHITE, "灰阶末格为白色");
    bool grayscale_ok = true;
    for (int x = 0; x < 319; x++) {
        if (Sim_Pixel(x, 240) > Sim_Pixel(x + 1, 240)) grayscale_ok = false;
    }
    check(grayscale_ok, "24 级灰阶单调递增");
    Sim_WritePng("test/out/4-grayscale.png");

    advance(3000);
    check(Sim_Pixel(0, 0) == BLACK, "手写页背景为黑色");
    check(Sim_Pixel(265, 455) == GRAY, "右下角 Clear 按钮已画出");

    Wire.deviceSetTouch(0x38, 1, 50, 100);
    advance(10);
    Wire.deviceSetTouch(0x38, 1, 60, 100);
    advance(10);
    check(Sim_Pixel(55, 100) == WHITE, "触摸移动后画出白色笔画");

    Wire.deviceSetTouch(0x38, 1, 300, 470);
    advance(10);
    check(Sim_Pixel(55, 100) == BLACK, "点击 Clear 后笔画清除");
    check(Sim_Pixel(265, 455) == GRAY && Sim_BacklightLevel() == 255,
          "清屏后按钮恢复且背光打开");
    Sim_WritePng("test/out/5-handwriting.png");

    Wire.deviceSetTouch(0x38, 0, 0, 0);
    /* 源工程测试在这里第二次调用 DemoApp_Setup() 模拟重启；F410 的背光由固件驱动，
     * 这次 setup 会按语义把 D6 再拉高一次，因此末尾按“重启只多写一次”判定。 */
    const int backlight_before_restart = Sim_BacklightWriteCount();
    DemoApp_Setup();
    Sim_ResetPixelCounter();
    Wire.deviceSetTouch(0x38, 1, 100, 200);
    advance(10);
    check(Sim_Pixel(0, 0) == BLACK && Sim_Pixel(265, 455) == GRAY,
          "演示播放期间轻触可立即跳到手写页");
    check(Sim_BacklightWriteCount() == backlight_before_restart + 1,
          "整个演示期间背光只在初始化和两次 setup 时写入（D6 访问 %d 次）",
          Sim_BacklightWriteCount());

    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
