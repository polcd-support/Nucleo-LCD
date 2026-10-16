#include "demo_app.h"

#include <Arduino.h>

#include "../screen/assets/logo.h"
#include "../screen/gfx/gfx.h"
#include "../screen/screen.h"

/* 时间参数与 STM32 参考工程 lcd.h 一致。 */
#define LOGO_DURATION 3000
#define TEXT_DURATION 2000
#define COLOR_FULL_INTERVAL 1000
#define EFFECT_DURATION 3000
#define TOUCH_POLL_MS 10
#define BTN_WIDTH 60
#define BTN_HEIGHT 30
#define NO_TOUCH 0xFFFF

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
static uint8_t s_color_index = 0;
static uint16_t s_last_x = NO_TOUCH;
static uint16_t s_last_y = NO_TOUCH;
static uint32_t s_last_poll = 0;
static bool s_touch_was_down = false;
static bool s_clear_pressed = false;
#if SCREEN_DEBUG
static uint32_t s_last_touch_log = 0;
#endif

static void finish_page_draw(DemoState state)
{
    s_state = state;
    /* 显示持续时间从画面绘制完成后开始计算，不包含慢速 SPI 绘制时间。 */
    s_state_start = millis();
}

static void show_logo(void)
{
    LCD_Fill(BLACK);
    LCD_ShowImage(0, 0, &kLogoImage);
    finish_page_draw(STATE_LOGO);
}

static void show_text(void)
{
    LCD_Fill(BLACK);
    LCD_ShowString(20, 50, "Nucleo F410RB", WHITE, BLACK, 24);
    LCD_ShowString(30, 100, "Multi-Size Text", BLUE, BLACK, 16);
    LCD_ShowChinese(80, 150, "浦洋液晶", RED, BLACK, 32);
    finish_page_draw(STATE_TEXT);
}

static void show_solid_color(uint8_t index)
{
    static const uint16_t colors[5] = {RED, GREEN, BLUE, WHITE, BLACK};

    LCD_Fill(colors[index]);
    finish_page_draw(STATE_COLOR_FULL);
}

static void show_color_bars(void)
{
    static const uint16_t colors[8] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, BLACK, WHITE};

    for (uint8_t i = 0; i < 8; i++) {
        LCD_FillRect((uint16_t)(i * 40), 0, (uint16_t)((i + 1) * 40 - 1),
                     (uint16_t)(Screen_Height() - 1), colors[i]);
    }
    finish_page_draw(STATE_COLOR_BAR);
}

static void show_grayscale(void)
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
    finish_page_draw(STATE_GRAYSCALE);
}

static void draw_clear_button(void)
{
    LCD_FillRect((uint16_t)(Screen_Width() - BTN_WIDTH),
                 (uint16_t)(Screen_Height() - BTN_HEIGHT),
                 (uint16_t)(Screen_Width() - 1),
                 (uint16_t)(Screen_Height() - 1), GRAY);
    LCD_ShowString((uint16_t)(Screen_Width() - BTN_WIDTH + 5),
                   (uint16_t)(Screen_Height() - BTN_HEIGHT + 8),
                   "Clear", BLACK, GRAY, 16);
}

static bool touch_in_clear_button(uint16_t x, uint16_t y)
{
    return x >= (uint16_t)(Screen_Width() - BTN_WIDTH) &&
           y >= (uint16_t)(Screen_Height() - BTN_HEIGHT);
}

static void show_handwriting(void)
{
    LCD_Fill(BLACK);
    draw_clear_button();
    finish_page_draw(STATE_HANDWRITING);
    s_last_x = NO_TOUCH;
    s_last_y = NO_TOUCH;
    s_clear_pressed = false;
}

void DemoApp_Setup(void)
{
    if (!Screen_Init()) {
        Serial.println(F("screen init failed"));
        for (;;) delay(1000);
    }

    s_color_index = 0;
    s_last_x = NO_TOUCH;
    s_last_y = NO_TOUCH;
    s_last_poll = millis();
    s_touch_was_down = false;
    s_clear_pressed = false;
    show_logo();

    /* UNO R4 板上的背光由硬件飞线强制拉高（板级 SCREEN_PIN_BL = -1）；Nucleo 这边
     * D6/PB10 是空闲脚，改由固件驱动：首屏画完再点亮，避免看到上电花屏与逐行推进。 */
    Screen_Backlight(true);
}

void DemoApp_Loop(void)
{
    const uint32_t now = millis();
    bool fresh_touch = false;
    bool touch_down = false;
    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    if (now - s_last_poll >= TOUCH_POLL_MS) {
        s_last_poll = now;
        fresh_touch = true;
        touch_down = Screen_TouchRead(&touch_x, &touch_y) > 0;
#if SCREEN_DEBUG
        if (touch_down && now - s_last_touch_log >= 100) {
            s_last_touch_log = now;
            Serial.print(F("touch: x="));
            Serial.print(touch_x);
            Serial.print(F(" y="));
            Serial.print(touch_y);
            Serial.println();
        }
#endif
    }

    /* 演示播放期间轻触屏幕可立即进入手写页，便于直接验证触摸。 */
    if (fresh_touch && s_state != STATE_HANDWRITING && touch_down && !s_touch_was_down) {
        show_handwriting();
        s_touch_was_down = true;
        s_clear_pressed = touch_in_clear_button(touch_x, touch_y);
        return;
    }

    switch (s_state) {
        case STATE_LOGO:
            if (now - s_state_start >= LOGO_DURATION) show_text();
            break;

        case STATE_TEXT:
            if (now - s_state_start >= TEXT_DURATION) {
                s_color_index = 0;
                show_solid_color(s_color_index);
            }
            break;

        case STATE_COLOR_FULL:
            if (now - s_state_start >= COLOR_FULL_INTERVAL) {
                s_color_index++;
                if (s_color_index < 5) show_solid_color(s_color_index);
                else show_color_bars();
            }
            break;

        case STATE_COLOR_BAR:
            if (now - s_state_start >= EFFECT_DURATION) show_grayscale();
            break;

        case STATE_GRAYSCALE:
            if (now - s_state_start >= EFFECT_DURATION) show_handwriting();
            break;

        case STATE_HANDWRITING:
            if (!fresh_touch) break;
            if (!touch_down) {
                s_last_x = NO_TOUCH;
                s_last_y = NO_TOUCH;
                s_clear_pressed = false;
                break;
            }

            if (touch_in_clear_button(touch_x, touch_y)) {
                if (!s_clear_pressed) {
                    show_handwriting();
                }
                s_clear_pressed = true;
                break;
            }
            s_clear_pressed = false;

            if (s_last_x != NO_TOUCH && s_last_y != NO_TOUCH) {
                LCD_DrawThickLine(s_last_x, s_last_y, touch_x, touch_y, WHITE, 2);
            }
            s_last_x = touch_x;
            s_last_y = touch_y;
            break;
    }

    if (fresh_touch) s_touch_was_down = touch_down;
}
