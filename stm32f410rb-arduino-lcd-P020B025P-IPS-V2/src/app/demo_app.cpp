#include "demo_app.h"

#include <Arduino.h>

#include "../screen/gfx/gfx.h"
#include "../screen/screen.h"

/* 时间参数与参考工程 lcd_demo.c 中的 3000 ms 延时一致。 */
#define EFFECT_DURATION 3000

enum DemoState {
    STATE_COLOR_BAR,
    STATE_GRAYSCALE,
};

static DemoState s_state = STATE_COLOR_BAR;
static uint32_t s_state_start = 0;
static bool s_state_drawn = false;

static void enter_state(DemoState state)
{
    s_state = state;
    s_state_start = millis();
    s_state_drawn = false;
}

/* 6 条竖彩条，颜色与条宽公式（i*W/6 .. (i+1)*W/6-1）与参考工程 DrawColorBars() 一致。 */
static void draw_color_bars(void)
{
    static const uint16_t colors[6] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
    const uint8_t bars = 6;
    for (uint8_t i = 0; i < bars; i++) {
        const uint16_t start = (uint16_t)((uint32_t)i * Screen_Width() / bars);
        const uint16_t end = (uint16_t)((uint32_t)(i + 1) * Screen_Width() / bars - 1);
        LCD_FillRect(start, 0, end, (uint16_t)(Screen_Height() - 1), colors[i]);
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

    LCD_Fill(BLACK);
    delay(100);
    Screen_Backlight(true);

    enter_state(STATE_COLOR_BAR);
}

void DemoApp_Loop(void)
{
    const uint32_t now = millis();

    switch (s_state) {
        case STATE_COLOR_BAR:
            if (!s_state_drawn) {
                draw_color_bars();
                s_state_drawn = true;
            }
            if (now - s_state_start > EFFECT_DURATION) {
                enter_state(STATE_GRAYSCALE);
            }
            break;

        case STATE_GRAYSCALE:
            if (!s_state_drawn) {
                draw_grayscale();
                s_state_drawn = true;
            }
            if (now - s_state_start > EFFECT_DURATION) {
                enter_state(STATE_COLOR_BAR);
            }
            break;
    }
}
