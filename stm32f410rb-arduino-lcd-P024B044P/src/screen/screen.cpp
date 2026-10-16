/*
 * 屏幕模块实现：面板/触摸无关的部分都在这里。
 *
 * 职责：
 *  1. 按 screen_devices.h 选中的设备初始化引脚时序、SPI 和 I2C。
 *  2. 把上层给的 RGB565 颜色转换成面板要求的像素格式，并分块发送。
 *  3. 旋转、GRAM 偏移、触摸坐标换算。
 *
 * 具体面板的寄存器序列在 panels 目录，具体触摸 IC 的寄存器在 touch 目录。
 *
 * 本文件是设备无关的：面板差异通过 ScreenPanel 描述，总线差异通过 SCREEN_SPI_DMA
 * 选中的后端（src/screen/bus/lcd_spi_stm32f4.cpp，SPI1 + DMA2）提供。
 */
#include "screen.h"

#include <Arduino.h>
#include <string.h>
#include "screen_devices.h"

/* 硬件后端只在实物固件里编译（Keil/armclang 定义了 STM32F410Rx）。
 * 主机端测试用 g++（test/run.sh）没有该宏，因此只编译逐字节回退路径，
 * 与移植前用 ARDUINO_ARCH_RENESAS_UNO 区分的做法同理。 */
#if SCREEN_SPI_DMA && defined(STM32F410Rx)
#define SCREEN_SPI_DMA_BACKEND 1
#else
#define SCREEN_SPI_DMA_BACKEND 0
#endif

#if SCREEN_SPI_DMA_BACKEND
#include "bus/lcd_spi_stm32f4.h"
#endif

static const ScreenDevice *s_device = nullptr;
static const ScreenPanel *s_panel = nullptr;
static uint8_t s_rotation = SCREEN_DEFAULT_ROTATION;
static uint16_t s_width = 0;
static uint16_t s_height = 0;
static int16_t s_offset_x = 0;
static int16_t s_offset_y = 0;
static uint32_t s_spi_hz = 0;
static uint8_t s_bpp = 2;
static SPISettings s_spi_settings;
static bool s_bus_open = false;
static bool s_ready = false;
static bool s_spi_dma = false;

/* 发送缓冲，同时作为“颜色重复填充”的模板缓冲。 */
alignas(4) static uint8_t s_tx[SCREEN_TX_BYTES];

/* ------------------------------------------------------------------ 总线 */

static void bus_enter(void)
{
    if (s_bus_open) return;
    if (!s_spi_dma) SCREEN_SPI_BUS.beginTransaction(s_spi_settings);
    digitalWrite(SCREEN_PIN_CS, LOW);
    s_bus_open = true;
}

static void bus_leave(void)
{
    if (!s_bus_open) return;
    digitalWrite(SCREEN_PIN_CS, HIGH);
    if (!s_spi_dma) SCREEN_SPI_BUS.endTransaction();
    s_bus_open = false;
}

static void write_command(uint8_t command)
{
    digitalWrite(SCREEN_PIN_DC, LOW);
#if SCREEN_SPI_DMA_BACKEND
    if (s_spi_dma) {
        LcdSpi_WriteDma(&command, 1);
    } else
#endif
    SCREEN_SPI_BUS.transfer(command);
    digitalWrite(SCREEN_PIN_DC, HIGH);
}

static void write_data(const uint8_t *data, uint32_t length)
{
    if (length == 0) return;
    digitalWrite(SCREEN_PIN_DC, HIGH);
#if SCREEN_SPI_DMA_BACKEND
    if (s_spi_dma) {
        if (LcdSpi_WriteDma(data, length)) return;
        /* 硬件后端出错时不能在同一条 SPI1 上混用两种发送方式；本次停止发送。 */
        return;
    }
#endif
#if SCREEN_SPI_BYTE_TRANSFER
    /* 稳定性优先：始终使用 8 位传输。真正的硬件搬运由 SPI1 + DMA2 完成，
     * 这里只是 DMA 后端不可用时的逐字节回退路径。 */
    while (length-- > 0) {
        SCREEN_SPI_BUS.transfer(*data++);
    }
#else
    /* 批量 API 会把接收数据写回缓冲区，常量数据必须先复制。 */
    while (length > 0) {
        const uint32_t take = (length < SCREEN_TX_BYTES) ? length : SCREEN_TX_BYTES;
        if (data != s_tx) memcpy(s_tx, data, take);
        SCREEN_SPI_BUS.transfer(s_tx, (size_t)take);
        data += take;
        length -= take;
    }
#endif
}

static void write_data8(uint8_t value)
{
    write_data(&value, 1);
}

/* --------------------------------------------------------------- 像素格式 */

static void pack_color(uint16_t color, uint8_t *pattern)
{
    if (s_bpp == SCREEN_PIXFMT_RGB666) {
        /* ILI9488 的 4 线 SPI 为 18-bit：R/G/B 各 6 位，低位补 0。 */
        pattern[0] = (uint8_t)((color >> 8) & 0xF8);
        pattern[1] = (uint8_t)((color >> 3) & 0xFC);
        pattern[2] = (uint8_t)((color << 3) & 0xF8);
    } else {
        pattern[0] = (uint8_t)(color >> 8);
        pattern[1] = (uint8_t)(color & 0xFF);
        pattern[2] = 0;
    }
}

static uint32_t chunk_pixels(void)
{
    return SCREEN_TX_BYTES / s_bpp;
}

/* 连续写入同一个颜色，不改变窗口。 */
static void fill_repeat(uint16_t color, uint32_t pixels)
{
    uint8_t pattern[3];
    pack_color(color, pattern);

    const uint32_t per_chunk = chunk_pixels();
    const uint32_t template_pixels = (pixels < per_chunk) ? pixels : per_chunk;
    const uint32_t template_bytes = template_pixels * s_bpp;
    for (uint32_t i = 0; i < template_bytes; i += s_bpp) {
        s_tx[i] = pattern[0];
        if (s_bpp > 1) s_tx[i + 1] = pattern[1];
        if (s_bpp > 2) s_tx[i + 2] = pattern[2];
    }

    /* 单色块只生成一次模板，后续由 DMA 反复发送同一缓冲区。 */
    while (pixels > 0) {
        const uint32_t count = (pixels < per_chunk) ? pixels : per_chunk;
        const uint32_t bytes = count * s_bpp;
        write_data(s_tx, bytes);
        pixels -= count;
    }
}

/* ---------------------------------------------------------------- 旋转 */

static void apply_rotation(uint8_t rotation)
{
    s_rotation = (uint8_t)(rotation & 0x03);
    if (s_rotation < 2) {
        /* 竖屏：0 / 180 度 */
        s_width = s_panel->width;
        s_height = s_panel->height;
    } else {
        /* 横屏：显存行列互换 */
        s_width = s_panel->height;
        s_height = s_panel->width;
    }
    s_offset_x = s_panel->offset_x[s_rotation];
    s_offset_y = s_panel->offset_y[s_rotation];

    bus_enter();
    write_command(0x36);
    write_data8(s_panel->madctl[s_rotation]);
    bus_leave();
}

void Screen_SetRotation(uint8_t rotation)
{
    if (!s_panel) return;
    apply_rotation(rotation);
}

/* ---------------------------------------------------------------- 初始化 */

static bool select_device(void)
{
    if (SCREEN_DEVICE >= SCREEN_DEVICE_COUNT) return false;
    s_device = SCREEN_DEVICE_TABLE[SCREEN_DEVICE];
    s_panel = s_device->panel;
    s_bpp = s_panel->pixel_format;
    s_spi_hz = s_panel->spi_hz < SCREEN_SPI_HZ ? s_panel->spi_hz : SCREEN_SPI_HZ;

    switch (s_panel->spi_mode & 0x03) {
        case 1:  s_spi_settings = SPISettings(s_spi_hz, MSBFIRST, SPI_MODE1); break;
        case 2:  s_spi_settings = SPISettings(s_spi_hz, MSBFIRST, SPI_MODE2); break;
        case 3:  s_spi_settings = SPISettings(s_spi_hz, MSBFIRST, SPI_MODE3); break;
        default: s_spi_settings = SPISettings(s_spi_hz, MSBFIRST, SPI_MODE0); break;
    }
    return true;
}

static void panel_hardware_reset(void)
{
    digitalWrite(SCREEN_PIN_RST, LOW);
    delay(100);
    digitalWrite(SCREEN_PIN_RST, HIGH);
    delay(100);
}

static void panel_run_init_sequence(void)
{
    bus_enter();
    for (uint16_t i = 0; i < s_panel->init_count; i++) {
        const ScreenPanelReg *reg = &s_panel->init[i];
        write_command(reg->command);
        if (reg->length > 0) write_data(reg->data, reg->length);
        if (reg->delay_ms > 0) {
            bus_leave();
            delay(reg->delay_ms);
            bus_enter();
        }
    }
    bus_leave();
}

bool Screen_Init(void)
{
    if (s_ready) return true;
    if (!select_device()) return false;

#if SCREEN_DEBUG
    /* 必须在触摸驱动打印芯片 ID 之前启动 USB Serial。 */
    Serial.begin(115200);
#endif

    /* 1) 转换器 OE 固定接 3.3 V；尽早给 LCD 控制脚确定电平。 */
#if SCREEN_PIN_BL >= 0
    pinMode(SCREEN_PIN_BL, OUTPUT);
#endif
    pinMode(SCREEN_PIN_RST, OUTPUT);
    pinMode(SCREEN_PIN_DC, OUTPUT);
    pinMode(SCREEN_PIN_CS, OUTPUT);

#if SCREEN_PIN_BL >= 0
    digitalWrite(SCREEN_PIN_BL, LOW);
#endif
    digitalWrite(SCREEN_PIN_RST, LOW);
    digitalWrite(SCREEN_PIN_DC, HIGH);
    digitalWrite(SCREEN_PIN_CS, HIGH);

    /* 触摸复位脚先按住低电平，稍后由驱动释放。 */
#if SCREEN_PIN_TOUCH_RST >= 0
    pinMode(SCREEN_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(SCREEN_PIN_TOUCH_RST, LOW);
#endif
#if SCREEN_PIN_TOUCH_INT >= 0
    pinMode(SCREEN_PIN_TOUCH_INT, INPUT);
#endif

#if SCREEN_SPI_DMA_BACKEND
    s_spi_dma = LcdSpi_Begin(s_spi_hz, s_panel->spi_mode & 0x03);
    if (!s_spi_dma) SCREEN_SPI_BUS.begin();
#else
    SCREEN_SPI_BUS.begin();
#endif
    SCREEN_I2C_BUS.begin();
    SCREEN_I2C_BUS.setClock(SCREEN_I2C_HZ);
    delay(20);

    /* 2) 面板复位与初始化，然后按默认方向写 MADCTL。 */
    panel_hardware_reset();
    panel_run_init_sequence();
    apply_rotation(SCREEN_DEFAULT_ROTATION);
    s_ready = true;

    /* 3) 触摸初始化。 */
    if (s_device->touch && s_device->touch->driver) {
        s_device->touch->driver->init();
    }

#if SCREEN_DEBUG
    Serial.print(F("screen: "));
    Serial.print(s_device->name);
    Serial.print(F("  panel="));
    Serial.print(s_panel->name);
    Serial.print(F("  "));
    Serial.print(s_width);
    Serial.print('x');
    Serial.print(s_height);
    Serial.print(F("  rotation="));
    Serial.print(s_rotation);
    Serial.print(F("  spi="));
    Serial.print(s_spi_hz / 1000000UL);
    Serial.print(F("MHz  bpp="));
    Serial.print(s_bpp);
    Serial.print(F("  tx="));
    Serial.print(s_spi_dma ? F("DMA") : F("byte"));
#if SCREEN_SPI_DMA_BACKEND
    if (s_spi_dma) {
        Serial.print(F("  actual="));
        const uint32_t actual_hz = LcdSpi_ActualHz();
        Serial.print(actual_hz / 1000000UL);
        Serial.print('.');
        Serial.print((actual_hz % 1000000UL) / 100000UL);
        Serial.print(F("MHz"));
    } else {
        Serial.print(F("(err="));
        Serial.print(LcdSpi_LastError());
        Serial.print(')');
    }
#endif
    Serial.print(F("  touch="));
    if (s_device->touch && s_device->touch->driver) {
        Serial.print(s_device->touch->driver->name);
        Serial.print(F(" @0x"));
        Serial.print(s_device->touch->driver->address, HEX);
    } else {
        Serial.print(F("none"));
    }
    Serial.println();
#endif

    return true;
}

/* ---------------------------------------------------------------- 基本量 */

const char *Screen_Name(void) { return s_device ? s_device->name : "none"; }
uint16_t Screen_Width(void) { return s_width; }
uint16_t Screen_Height(void) { return s_height; }
uint8_t Screen_Rotation(void) { return s_rotation; }

void Screen_BacklightLevel(uint8_t level)
{
#if SCREEN_PIN_BL < 0
    /* 背光由硬件强制拉高，固件不得配置或驱动 D6。 */
    (void)level;
#else
#if SCREEN_BL_PWM
    analogWrite(SCREEN_PIN_BL, level);
#else
    digitalWrite(SCREEN_PIN_BL, level ? HIGH : LOW);
#endif
#endif
}

void Screen_Backlight(bool on)
{
    Screen_BacklightLevel(on ? 255 : 0);
}

/* ---------------------------------------------------------------- 绘制 */

void Screen_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (!s_ready) return;

    bus_enter();
    const uint16_t cx0 = (uint16_t)(x0 + s_offset_x);
    const uint16_t cx1 = (uint16_t)(x1 + s_offset_x);
    const uint16_t cy0 = (uint16_t)(y0 + s_offset_y);
    const uint16_t cy1 = (uint16_t)(y1 + s_offset_y);

    const uint8_t columns[4] = {
        (uint8_t)(cx0 >> 8), (uint8_t)cx0,
        (uint8_t)(cx1 >> 8), (uint8_t)cx1,
    };
    const uint8_t rows[4] = {
        (uint8_t)(cy0 >> 8), (uint8_t)cy0,
        (uint8_t)(cy1 >> 8), (uint8_t)cy1,
    };

    write_command(0x2A);
    write_data(columns, sizeof(columns));

    write_command(0x2B);
    write_data(rows, sizeof(rows));

    write_command(0x2C);
    /* CS 保持低，等像素数据；由 Screen_WritePixels565()/Screen_Fill*() 释放。 */
}

void Screen_WritePixels565(const uint16_t *pixels, uint32_t count)
{
    if (!s_ready) return;

    bus_enter();
    const uint32_t per_chunk = chunk_pixels();
    while (count > 0) {
        const uint32_t n = (count < per_chunk) ? count : per_chunk;
        uint32_t i = 0;
        for (uint32_t p = 0; p < n; p++) {
            const uint16_t color = pixels[p];
            if (s_bpp == SCREEN_PIXFMT_RGB666) {
                s_tx[i++] = (uint8_t)((color >> 8) & 0xF8);
                s_tx[i++] = (uint8_t)((color >> 3) & 0xFC);
                s_tx[i++] = (uint8_t)((color << 3) & 0xF8);
            } else {
                s_tx[i++] = (uint8_t)(color >> 8);
                s_tx[i++] = (uint8_t)(color & 0xFF);
            }
        }
        write_data(s_tx, i);
        pixels += n;
        count -= n;
    }
    bus_leave();
}

void Screen_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (!s_ready) return;

    if (x0 > x1) { const uint16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { const uint16_t t = y0; y0 = y1; y1 = t; }
    if (x0 >= s_width || y0 >= s_height) return;
    if (x1 >= s_width) x1 = (uint16_t)(s_width - 1);
    if (y1 >= s_height) y1 = (uint16_t)(s_height - 1);

    Screen_SetWindow(x0, y0, x1, y1);
    fill_repeat(color, (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1));
    bus_leave();
}

void Screen_Fill(uint16_t color)
{
    Screen_FillRect(0, 0, (uint16_t)(s_width - 1), (uint16_t)(s_height - 1), color);
}

/* ---------------------------------------------------------------- 触摸 */

bool Screen_HasTouch(void)
{
    return s_device && s_device->touch && s_device->touch->driver;
}

uint8_t Screen_TouchReadRaw(uint16_t *x, uint16_t *y)
{
    if (!Screen_HasTouch()) return 0;
    return s_device->touch->driver->read(x, y);
}

uint8_t Screen_TouchRead(uint16_t *x, uint16_t *y)
{
    if (!Screen_HasTouch()) return 0;

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    if (s_device->touch->driver->read(&raw_x, &raw_y) == 0) return 0;

    const ScreenTouchConfig *cfg = s_device->touch;
    int32_t nx = raw_x;
    int32_t ny = raw_y;

    if (cfg->invert_x) nx = (int32_t)cfg->raw_width - 1 - nx;
    if (cfg->invert_y) ny = (int32_t)cfg->raw_height - 1 - ny;
    if (cfg->swap_xy) {
        const int32_t t = nx;
        nx = ny;
        ny = t;
    }
    nx += cfg->offset_x;
    ny += cfg->offset_y;

    /* 夹到面板原生范围，避免配置错误时算出离谱坐标。 */
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > (int32_t)s_panel->width - 1) nx = (int32_t)s_panel->width - 1;
    if (ny > (int32_t)s_panel->height - 1) ny = (int32_t)s_panel->height - 1;

    /* 原生坐标 -> 当前显示方向。 */
    const int32_t pw = s_panel->width;
    const int32_t ph = s_panel->height;
    int32_t dx = nx;
    int32_t dy = ny;
    switch (s_rotation) {
        case 1: dx = pw - 1 - nx; dy = ph - 1 - ny; break;
        case 2: dx = ny;          dy = nx;          break;
        case 3: dx = ph - 1 - ny; dy = pw - 1 - nx; break;
        default: break;
    }

    *x = (uint16_t)dx;
    *y = (uint16_t)dy;
    return 1;
}
