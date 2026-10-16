#include "demo_app.h"

#include <Arduino.h>

#include "../screen/assets/logo.h"
#include "../screen/gfx/gfx.h"
#include "../screen/screen.h"

/* 时间参数与参考工程 lcd.h 中的宏一致。 */
#define LOGO_DURATION 3000
#define TEXT_DURATION 2000
#define COLOR_FULL_INTERVAL 1000
#define EFFECT_DURATION 3000

/* 参考工程 lcd_demo.c 里的 LOGO 尺寸（gImage_logo 去掉 8 字节图像头后 240x220）。 */
#define LOGO_W 240
#define LOGO_H 220

/* 参考工程 lcd.h：彩条 6 条，条宽 = 屏宽/6。 */
#define COLOR_BAR_NUM 6

enum DemoState {
    STATE_LOGO,
    STATE_TEXT,
    STATE_COLOR_FULL,
    STATE_COLOR_BAR,
    STATE_GRAYSCALE,
};

static DemoState s_state = STATE_LOGO;
static uint32_t s_state_start = 0;
static bool s_state_drawn = false;
static uint8_t s_color_index = 0;

static void enter_state(DemoState state)
{
    s_state = state;
    s_state_start = millis();
    s_state_drawn = false;
}

/* 6 条竖彩条，颜色与参考工程 DrawColorBars() 一致。 */
static void draw_color_bars(void)
{
    static const uint16_t colors[COLOR_BAR_NUM] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    const uint16_t bar = (uint16_t)(Screen_Width() / COLOR_BAR_NUM);
    for (uint8_t i = 0; i < COLOR_BAR_NUM; i++) {
        LCD_FillRect((uint16_t)(i * bar), 0, (uint16_t)((i + 1) * bar - 1), (uint16_t)(Screen_Height() - 1), colors[i]);
    }
}

/* 24 级灰阶，与参考工程 DrawGrayscale() 一致（余数补在最后一格）。 */
static void draw_grayscale(void)
{
    const uint8_t levels = 24;
    const uint16_t width = (uint16_t)(Screen_Width() / levels);
    const uint16_t remainder = (uint16_t)(Screen_Width() % levels);

    for (uint8_t n = 0; n < levels; n++) {
        const uint16_t start = (uint16_t)(n * width);
        uint16_t end = (uint16_t)(start + width - 1);
        if (n == levels - 1) end = (uint16_t)(end + remainder);
        const uint8_t gray = (uint8_t)((n * 255) / (levels - 1));
        LCD_FillRect(start, 0, end, (uint16_t)(Screen_Height() - 1), RGB565(gray, gray, gray));
    }
}

void DemoApp_Setup(void)
{
    if (!Screen_Init()) {
        Serial.println(F("screen init failed"));
        for (;;) delay(1000);
    }

    /* 参考工程 LCD_DEMO()：初始化 -> 全屏黑 -> 100 ms -> 开背光 -> 进入演示循环。 */
    LCD_Fill(BLACK);
    delay(100);
    Screen_Backlight(true);

    s_color_index = 0;
    enter_state(STATE_LOGO);
}

void DemoApp_Loop(void)
{
    const uint32_t now = millis();

    switch (s_state) {
        case STATE_LOGO:
            if (!s_state_drawn) {
                /* 参考工程：LCD_ShowPicture(0, (240-220)/2, 240, 220, gImage_logo + 8)。 */
                LCD_ShowImage(0, (uint16_t)((Screen_Height() - LOGO_H) / 2), &kLogoImage);
                s_state_drawn = true;
            }
            if (now - s_state_start > LOGO_DURATION) {
                LCD_Fill(BLACK);
                enter_state(STATE_TEXT);
            }
            break;

        case STATE_TEXT:
            if (!s_state_drawn) {
                LCD_Fill(BLACK);
                LCD_ShowString(42, 52, "Nucleo F410RB", WHITE, BLACK, 24);
                LCD_ShowString(56, 100, "240x240 IPS", BLUE, BLACK, 16);
                LCD_ShowString(56, 124, "ST7789P3 SPI", BLUE, BLACK, 16);
                LCD_ShowChinese(56, 168, "浦洋液晶", RED, BLACK, 32);
                s_state_drawn = true;
            }
            if (now - s_state_start > TEXT_DURATION) {
                enter_state(STATE_COLOR_FULL);
            }
            break;

        case STATE_COLOR_FULL: {
            /* 参考工程：红/绿/蓝/白/黑，各停留 COLOR_FULL_INTERVAL。 */
            static const uint16_t colors[5] = {RED, GREEN, BLUE, WHITE, BLACK};
            if (!s_state_drawn) {
                LCD_Fill(colors[s_color_index]);
                s_state_drawn = true;
            }
            if (now - s_state_start > COLOR_FULL_INTERVAL) {
                if (++s_color_index > 4) {
                    s_color_index = 0;
                    enter_state(STATE_COLOR_BAR);
                } else {
                    enter_state(STATE_COLOR_FULL);
                }
            }
            break;
        }

        case STATE_COLOR_BAR:
            if (!s_state_drawn) {
                LCD_Fill(BLACK);
                draw_color_bars();
                s_state_drawn = true;
            }
            if (now - s_state_start > EFFECT_DURATION) {
                LCD_Fill(BLACK);
                enter_state(STATE_GRAYSCALE);
            }
            break;

        case STATE_GRAYSCALE:
            if (!s_state_drawn) {
                draw_grayscale();
                s_state_drawn = true;
            }
            if (now - s_state_start > EFFECT_DURATION) {
                LCD_Fill(BLACK);
                enter_state(STATE_LOGO);
            }
            break;
    }
}
