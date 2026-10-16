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

/* 触摸采样间隔：CST816 活动模式上报周期约 10 ms。 */
#define TOUCH_POLL_MS 10

#define NO_TOUCH 0xFFFF

/* LOGO 为 200x183，参考工程 LCD_ShowPicture(29, 29, 199, 182, ...) 的起点。 */
#define LOGO_X 29
#define LOGO_Y 29

enum DemoState {
    STATE_LOGO,
    STATE_TEXT,
    STATE_COLOR_FULL,
    STATE_COLOR_BAR,
    STATE_GRAYSCALE,
    STATE_HANDWRITING,
};

static DemoState s_state = STATE_LOGO;
static uint32_t s_state_start = 0;
static bool s_state_drawn = false;
static uint8_t s_color_index = 0;
static uint16_t s_last_x = NO_TOUCH;
static uint16_t s_last_y = NO_TOUCH;
static uint32_t s_last_poll = 0;
static bool s_touch_down = false;
static uint16_t s_touch_x = 0;
static uint16_t s_touch_y = 0;

static void enter_state(DemoState state)
{
    s_state = state;
    s_state_start = millis();
    s_state_drawn = false;
}

/* 6 条竖彩条，颜色与条宽与参考工程 DrawColorBars() 一致（SCREEN_WIDTH/6 向上取整为 40）。 */
static void draw_color_bars(void)
{
    static const uint16_t colors[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    for (uint8_t i = 0; i < 6; i++) {
        LCD_FillRect((uint16_t)(i * 40), 0, (uint16_t)((i + 1) * 40 - 1), (uint16_t)(Screen_Height() - 1), colors[i]);
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

/* 参考工程进入手写画面只清屏，不画 Clear 按钮（DrawClearButton 被注释掉），
 * 按钮命中检测同样被注释掉，这里保持同样行为。 */
static void draw_handwriting_screen(void)
{
    LCD_Fill(BLACK);
}

void DemoApp_Setup(void)
{
    if (!Screen_Init()) {
        Serial.println(F("screen init failed"));
        for (;;) delay(1000);
    }

    LCD_Fill(BLACK);
    delay(100);
    Screen_Backlight(true);

    s_last_x = NO_TOUCH;
    s_last_y = NO_TOUCH;
    s_last_poll = millis();
    enter_state(STATE_LOGO);
}

void DemoApp_Loop(void)
{
    const uint32_t now = millis();
    bool fresh_sample = false;

    if (now - s_last_poll >= TOUCH_POLL_MS) {
        s_last_poll = now;
        fresh_sample = true;
        uint16_t tx = 0;
        uint16_t ty = 0;
        if (Screen_TouchRead(&tx, &ty) > 0) {
            s_touch_down = true;
            s_touch_x = tx;
            s_touch_y = ty;
        } else {
            s_touch_down = false;
        }
    }

    switch (s_state) {
        case STATE_LOGO:
            if (!s_state_drawn) {
                LCD_ShowImage(LOGO_X, LOGO_Y, &kLogoImage);
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
                LCD_ShowString(20, 50, "Nucleo F410RB", WHITE, BLACK, 24);
                LCD_ShowString(30, 100, "Multi-Size Text", BLUE, BLACK, 16);
                LCD_ShowChinese(80, 150, "浦洋液晶", RED, BLACK, 32);
                s_state_drawn = true;
            }
            if (now - s_state_start > TEXT_DURATION) {
                enter_state(STATE_COLOR_FULL);
            }
            break;

        case STATE_COLOR_FULL: {
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
                draw_handwriting_screen();
                enter_state(STATE_HANDWRITING);
            }
            break;

        case STATE_HANDWRITING:
            if (fresh_sample) {
                if (s_touch_down) {
                    if (s_last_x != NO_TOUCH && s_last_y != NO_TOUCH) {
                        LCD_DrawThickLine(s_last_x, s_last_y, s_touch_x, s_touch_y, WHITE, 2);
                    }
                    s_last_x = s_touch_x;
                    s_last_y = s_touch_y;
                } else {
                    s_last_x = NO_TOUCH;
                    s_last_y = NO_TOUCH;
                }
            }
            break;
    }
}
