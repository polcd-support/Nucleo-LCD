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
#define BTN_WIDTH 60
#define BTN_HEIGHT 30

/* 触摸采样间隔：FT6236 活动模式上报周期为 10 ms。 */
#define TOUCH_POLL_MS 10

#define NO_TOUCH 0xFFFF

/* 拨号画面布局常量，与参考工程 lcd_demo.c 一致。 */
#define DIAL_KEY_X0 12
#define DIAL_KEY_Y0 76
#define DIAL_KEY_W 64
#define DIAL_KEY_H 42
#define DIAL_KEY_X_GAP 12
#define DIAL_KEY_Y_GAP 6
#define DIAL_FUNC_Y 274
#define DIAL_HIT_MARGIN 8

enum DemoState {
    STATE_LOGO,
    STATE_TEXT,
    STATE_COLOR_FULL,
    STATE_COLOR_BAR,
    STATE_GRAYSCALE,
    STATE_HANDWRITING,
    STATE_PHONE_DIALER,
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

/* 拨号画面状态：号码、长度、抬手锁（进入画面时的那次按下不当作按键）。 */
static char s_dial_number[16] = {0};
static uint8_t s_dial_len = 0;
static bool s_dial_lock = false;

static void enter_state(DemoState state)
{
    s_state = state;
    s_state_start = millis();
    s_state_drawn = false;
}

/* 6 条竖彩条，颜色与条宽（W/6=40）与参考工程 DrawColorBars() 一致。 */
static void draw_color_bars(void)
{
    static const uint16_t colors[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    const uint16_t bar = (uint16_t)(Screen_Width() / 6);
    for (uint8_t i = 0; i < 6; i++) {
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

static void draw_clear_button(void)
{
    LCD_FillRect((uint16_t)(Screen_Width() - BTN_WIDTH), (uint16_t)(Screen_Height() - BTN_HEIGHT),
                 (uint16_t)(Screen_Width() - 1), (uint16_t)(Screen_Height() - 1), GRAY);
    LCD_ShowString((uint16_t)(Screen_Width() - BTN_WIDTH + 5), (uint16_t)(Screen_Height() - BTN_HEIGHT + 8),
                   "Clear", BLACK, GRAY, 16);
}

/* 参考工程 DrawHandwritingButtons()：Clear 按钮 + 左下角 Dial 按钮。 */
static void draw_handwriting_screen(void)
{
    LCD_Fill(BLACK);
    draw_clear_button();
    LCD_FillRect(0, (uint16_t)(Screen_Height() - BTN_HEIGHT - 10), (uint16_t)(BTN_WIDTH + 10),
                 (uint16_t)(Screen_Height() - 1), DARKBLUE);
    LCD_ShowString(12, (uint16_t)(Screen_Height() - BTN_HEIGHT + 3), "Dial", WHITE, DARKBLUE, 16);
}

static bool touch_in_button(uint16_t x, uint16_t y)
{
    return x >= (uint16_t)(Screen_Width() - BTN_WIDTH) && y >= (uint16_t)(Screen_Height() - BTN_HEIGHT);
}

static bool touch_in_dial_button(uint16_t x, uint16_t y)
{
    return x < (uint16_t)(BTN_WIDTH + 18) && y >= (uint16_t)(Screen_Height() - BTN_HEIGHT - 18);
}

/* ------------------------------------------------------------- 拨号画面 */

static bool point_in_rect(uint16_t x, uint16_t y, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh, uint16_t margin)
{
    const uint16_t x0 = (rx > margin) ? (uint16_t)(rx - margin) : 0;
    const uint16_t y0 = (ry > margin) ? (uint16_t)(ry - margin) : 0;
    const uint16_t x1 = (uint16_t)(rx + rw + margin);
    const uint16_t y1 = (uint16_t)(ry + rh + margin);
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

static void draw_dialer_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *text, uint16_t fill,
                               uint16_t text_color)
{
    LCD_FillRect(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1), fill);
    LCD_DrawRectangle(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1), WHITE);
    LCD_ShowString((uint16_t)(x + (w > 32 ? (w - 16) / 2 : 4)), (uint16_t)(y + (h - 16) / 2), text, text_color, fill, 16);
}

static void draw_dial_number(void)
{
    LCD_FillRect(8, 32, (uint16_t)(Screen_Width() - 9), 84, BLACK);
    LCD_DrawRectangle(8, 32, (uint16_t)(Screen_Width() - 9), 70, GRAY);
    LCD_ShowString(14, 44, s_dial_number, WHITE, BLACK, 24);
}

static void draw_phone_dialer(void)
{
    static const char *const keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};

    LCD_Fill(BLACK);
    LCD_ShowString(60, 8, "Phone Dial", WHITE, BLACK, 16);
    draw_dial_number();

    for (uint8_t i = 0; i < 12; i++) {
        const uint16_t row = (uint16_t)(i / 3);
        const uint16_t col = (uint16_t)(i % 3);
        draw_dialer_button((uint16_t)(DIAL_KEY_X0 + col * (DIAL_KEY_W + DIAL_KEY_X_GAP)),
                           (uint16_t)(DIAL_KEY_Y0 + row * (DIAL_KEY_H + DIAL_KEY_Y_GAP)),
                           DIAL_KEY_W, DIAL_KEY_H, keys[i], LGRAYBLUE, BLACK);
    }

    draw_dialer_button(12, DIAL_FUNC_Y, 64, 34, "Back", GRAY, BLACK);
    draw_dialer_button(88, DIAL_FUNC_Y, 64, 34, "Call", GREEN, BLACK);
    draw_dialer_button(164, DIAL_FUNC_Y, 64, 34, "Del", BRRED, WHITE);
}

/* 左上角 24 像素高的黑条 + X/Y 坐标（16px，3 位，高位补空格）。 */
static void draw_touch_debug(uint16_t x, uint16_t y)
{
    LCD_FillRect(0, 0, (uint16_t)(Screen_Width() - 1), 24, BLACK);
    LCD_ShowString(6, 4, "X", WHITE, BLACK, 16);
    LCD_ShowIntNum(22, 4, x, 3, WHITE, BLACK, 16);
    LCD_ShowString(82, 4, "Y", WHITE, BLACK, 16);
    LCD_ShowIntNum(98, 4, y, 3, WHITE, BLACK, 16);
}

static void handle_phone_dialer_touch(uint16_t x, uint16_t y)
{
    static const char key_chars[12] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#'};

    draw_touch_debug(x, y);

    for (uint8_t row = 0; row < 4; row++) {
        for (uint8_t col = 0; col < 3; col++) {
            const uint16_t key_x = (uint16_t)(DIAL_KEY_X0 + col * (DIAL_KEY_W + DIAL_KEY_X_GAP));
            const uint16_t key_y = (uint16_t)(DIAL_KEY_Y0 + row * (DIAL_KEY_H + DIAL_KEY_Y_GAP));
            if (point_in_rect(x, y, key_x, key_y, DIAL_KEY_W, DIAL_KEY_H, DIAL_HIT_MARGIN)) {
                if (s_dial_len < sizeof(s_dial_number) - 1) {
                    s_dial_number[s_dial_len++] = key_chars[row * 3 + col];
                    s_dial_number[s_dial_len] = '\0';
                    draw_dial_number();
                }
                return;
            }
        }
    }

    if (point_in_rect(x, y, 12, DIAL_FUNC_Y, 64, 34, DIAL_HIT_MARGIN)) {
        LCD_Fill(BLACK);
        draw_handwriting_screen();
        enter_state(STATE_HANDWRITING);
    } else if (point_in_rect(x, y, 88, DIAL_FUNC_Y, 64, 34, DIAL_HIT_MARGIN)) {
        LCD_FillRect(80, 70, 160, 92, BLACK);
        LCD_ShowString(88, 74, "Calling", GREEN, BLACK, 16);
    } else if (point_in_rect(x, y, 164, DIAL_FUNC_Y, 64, 34, DIAL_HIT_MARGIN)) {
        if (s_dial_len > 0) {
            s_dial_number[--s_dial_len] = '\0';
            draw_dial_number();
        }
    }
}

/* ------------------------------------------------------------- 主流程 */

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
                LCD_ShowImage(0, 29, &kLogoImage);
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
                    if (touch_in_button(s_touch_x, s_touch_y)) {
                        draw_handwriting_screen();
                        s_last_x = NO_TOUCH;
                        s_last_y = NO_TOUCH;
                    } else if (touch_in_dial_button(s_touch_x, s_touch_y)) {
                        LCD_Fill(BLACK);
                        s_dial_len = 0;
                        s_dial_number[0] = '\0';
                        s_dial_lock = true;
                        draw_phone_dialer();
                        enter_state(STATE_PHONE_DIALER);
                        s_last_x = NO_TOUCH;
                        s_last_y = NO_TOUCH;
                    } else {
                        if (s_last_x != NO_TOUCH && s_last_y != NO_TOUCH) {
                            LCD_DrawThickLine(s_last_x, s_last_y, s_touch_x, s_touch_y, WHITE, 2);
                        }
                        s_last_x = s_touch_x;
                        s_last_y = s_touch_y;
                    }
                } else {
                    s_last_x = NO_TOUCH;
                    s_last_y = NO_TOUCH;
                    s_dial_lock = false;
                }
            }
            break;

        case STATE_PHONE_DIALER:
            if (fresh_sample) {
                if (s_touch_down) {
                    if (!s_dial_lock) {
                        handle_phone_dialer_touch(s_touch_x, s_touch_y);
                        s_dial_lock = true;   /* 一次按下只处理一次，抬手后解锁 */
                    }
                } else {
                    s_dial_lock = false;
                }
            }
            break;
    }
}
