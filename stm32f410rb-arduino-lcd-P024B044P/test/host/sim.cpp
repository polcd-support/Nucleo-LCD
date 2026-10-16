/*
 * 虚拟面板：解析 4 线 SPI 字节流（0x2A/0x2B 设窗口，0x2C 后是像素），
 * 按选中面板的 pixel_format 解码（RGB565 2 字节 / RGB666 3 字节），
 * 维护一块 240x320 的 RGB565 显存，并可导出 PNG 供人工核对。
 */
#include "Arduino.h"
#include "sim.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/screen/screen_devices.h"

namespace {

uint16_t g_fb[SIM_WIDTH * SIM_HEIGHT];
SimCommandRecord g_commands[SIM_MAX_COMMANDS];
int g_command_count = 0;
int g_pixel_count = 0;
int g_first_spi_ms = 0;
bool g_first_spi_seen = false;
int g_backlight = 0;
int g_backlight_write_count = 0;

/* 背光脚就是板卡配置里的 SCREEN_PIN_BL（Nucleo-F410RB 上为 D6/PB10）。
 * 板卡把背光交给外部硬件时该值为 -1，转成 255 后不会与任何引脚号相撞。 */
const uint8_t kBacklightPin = (uint8_t)SCREEN_PIN_BL;

uint8_t g_cs = HIGH;
uint8_t g_dc = HIGH;
uint8_t g_rst = LOW;

uint8_t g_current_command = 0;
uint8_t g_params[4];
uint8_t g_param_count = 0;
bool g_streaming = false;
uint16_t g_x0 = 0;
uint16_t g_x1 = 0;
uint16_t g_y0 = 0;
uint16_t g_y1 = 0;
int g_cx = 0;
int g_cy = 0;
uint8_t g_pixel_bytes[3];
int g_pixel_byte_count = 0;
uint8_t g_pixel_format = SCREEN_PIXFMT_RGB565;

void record_command(uint8_t command)
{
    if (g_command_count >= SIM_MAX_COMMANDS) return;
    SimCommandRecord *record = &g_commands[g_command_count++];
    record->time_ms = millis();
    record->command = command;
    record->length = 0;
    std::memset(record->data, 0, sizeof(record->data));
}

void store_param(uint8_t value)
{
    if (g_param_count < 4) g_params[g_param_count] = value;
    g_param_count++;
    if (g_param_count == 4) {
        const uint16_t first = (uint16_t)((g_params[0] << 8) | g_params[1]);
        const uint16_t second = (uint16_t)((g_params[2] << 8) | g_params[3]);
        if (g_current_command == 0x2A) {
            g_x0 = first;
            g_x1 = second;
        } else if (g_current_command == 0x2B) {
            g_y0 = first;
            g_y1 = second;
        }
    }
}

void write_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (g_cx >= 0 && g_cx < SIM_WIDTH && g_cy >= 0 && g_cy < SIM_HEIGHT) {
        g_fb[g_cy * SIM_WIDTH + g_cx] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    g_pixel_count++;
    g_cx++;
    if (g_cx > (int)g_x1) {
        g_cx = g_x0;
        g_cy++;
        if (g_cy > (int)g_y1) g_cy = g_y0; /* 面板写下溢出后的行为：回到窗口首行 */
    }
}

/* RGB565：SPI 上高位字节在前，直接存。 */
void write_pixel565(uint8_t hi, uint8_t lo)
{
    if (g_cx >= 0 && g_cx < SIM_WIDTH && g_cy >= 0 && g_cy < SIM_HEIGHT) {
        g_fb[g_cy * SIM_WIDTH + g_cx] = (uint16_t)((hi << 8) | lo);
    }
    g_pixel_count++;
    g_cx++;
    if (g_cx > (int)g_x1) {
        g_cx = g_x0;
        g_cy++;
        if (g_cy > (int)g_y1) g_cy = g_y0;
    }
}

}  // namespace

void Sim_Reset(void)
{
    std::memset(g_fb, 0, sizeof(g_fb));
    g_command_count = 0;
    g_pixel_count = 0;
    g_first_spi_ms = 0;
    g_first_spi_seen = false;
    /* 背光脚由固件驱动：初始化时拉低，DemoApp_Setup() 里才拉高。 */
    g_backlight = 0;
    g_backlight_write_count = 0;
    g_cs = HIGH;
    g_dc = HIGH;
    g_rst = LOW;
    g_current_command = 0;
    g_param_count = 0;
    g_streaming = false;
    g_x0 = g_y0 = 0;
    g_x1 = SIM_WIDTH - 1;
    g_y1 = SIM_HEIGHT - 1;
    g_cx = g_cy = 0;
    g_pixel_byte_count = 0;
    g_pixel_format = SCREEN_DEVICE_TABLE[SCREEN_DEVICE]->panel->pixel_format;
}

void Sim_PinMode(uint8_t pin, uint8_t)
{
    if (pin == kBacklightPin) g_backlight_write_count++;
}

void Sim_PinWrite(uint8_t pin, uint8_t value)
{
    if (pin == SCREEN_PIN_CS) g_cs = value;
    else if (pin == SCREEN_PIN_DC) g_dc = value;
    else if (pin == SCREEN_PIN_RST) g_rst = value;
    else if (pin == kBacklightPin) {
        g_backlight_write_count++;
        g_backlight = value == HIGH ? 255 : 0;
    }
}

void Sim_AnalogWrite(uint8_t pin, int value)
{
    if (pin == kBacklightPin) {
        g_backlight_write_count++;
        g_backlight = value;
    }
}

void Sim_SpiByte(uint8_t value)
{
    if (!g_first_spi_seen) {
        g_first_spi_seen = true;
        g_first_spi_ms = millis();
    }
    if (g_cs != LOW) return; /* CS 无效时面板不采样 */

    if (g_dc == LOW) {
        g_current_command = value;
        g_param_count = 0;
        g_streaming = (value == 0x2C);
        record_command(value);
        if (g_streaming) {
            g_cx = g_x0;
            g_cy = g_y0;
            g_pixel_byte_count = 0;
        }
        return;
    }

    if (g_command_count > 0) {
        SimCommandRecord *record = &g_commands[g_command_count - 1];
        if (record->length < sizeof(record->data)) record->data[record->length++] = value;
    }

    if (g_streaming) {
        g_pixel_bytes[g_pixel_byte_count++] = value;
        if (g_pixel_byte_count == g_pixel_format) {
            g_pixel_byte_count = 0;
            if (g_pixel_format == SCREEN_PIXFMT_RGB565) {
                write_pixel565(g_pixel_bytes[0], g_pixel_bytes[1]);
            } else {
                write_pixel(g_pixel_bytes[0], g_pixel_bytes[1], g_pixel_bytes[2]);
            }
        }
    } else if (g_current_command == 0x2A || g_current_command == 0x2B) {
        store_param(value);
    }
}

uint16_t Sim_Pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= SIM_WIDTH || y >= SIM_HEIGHT) return 0;
    return g_fb[y * SIM_WIDTH + x];
}

int Sim_PixelCount(void) { return g_pixel_count; }
void Sim_ResetPixelCounter(void) { g_pixel_count = 0; }
int Sim_CommandCount(void) { return g_command_count; }
const SimCommandRecord *Sim_Command(int index)
{
    if (index < 0 || index >= g_command_count) return nullptr;
    return &g_commands[index];
}
uint32_t Sim_FirstSpiMs(void) { return g_first_spi_ms; }
int Sim_BacklightLevel(void) { return g_backlight; }
int Sim_BacklightWriteCount(void) { return g_backlight_write_count; }
int Sim_CurrentX(void) { return g_cx; }
int Sim_CurrentY(void) { return g_cy; }

/* ------------------------------------------------------------------ PNG */

namespace {

uint32_t crc_table[256];
bool crc_ready = false;

void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_ready = true;
}

uint32_t crc32_of(const uint8_t *data, size_t length)
{
    if (!crc_ready) crc_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void push_u32(std::vector<uint8_t> &out, uint32_t value)
{
    out.push_back((uint8_t)(value >> 24));
    out.push_back((uint8_t)(value >> 16));
    out.push_back((uint8_t)(value >> 8));
    out.push_back((uint8_t)value);
}

void push_chunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &payload)
{
    push_u32(out, (uint32_t)payload.size());
    const size_t start = out.size();
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)type[i]);
    out.insert(out.end(), payload.begin(), payload.end());
    push_u32(out, crc32_of(out.data() + start, out.size() - start));
}

/* 用 zlib 的 stored 块（不压缩），避免引入压缩库。 */
std::vector<uint8_t> zlib_store(const std::vector<uint8_t> &raw)
{
    std::vector<uint8_t> out;
    out.push_back(0x78);
    out.push_back(0x01);
    size_t offset = 0;
    while (offset < raw.size()) {
        size_t block = raw.size() - offset;
        if (block > 65535) block = 65535;
        const bool last = (offset + block) >= raw.size();
        out.push_back(last ? 1 : 0);
        const uint16_t len = (uint16_t)block;
        out.push_back((uint8_t)(len & 0xFF));
        out.push_back((uint8_t)(len >> 8));
        const uint16_t nlen = (uint16_t)~len;
        out.push_back((uint8_t)(nlen & 0xFF));
        out.push_back((uint8_t)(nlen >> 8));
        out.insert(out.end(), raw.begin() + offset, raw.begin() + offset + block);
        offset += block;
    }
    uint32_t a = 1;
    uint32_t b = 0;
    for (uint8_t value : raw) {
        a = (a + value) % 65521;
        b = (b + a) % 65521;
    }
    push_u32(out, (b << 16) | a);
    return out;
}

}  // namespace

void Sim_WritePng(const char *path)
{
    std::vector<uint8_t> raw;
    raw.reserve((size_t)SIM_HEIGHT * (1 + SIM_WIDTH * 3));
    for (int y = 0; y < SIM_HEIGHT; y++) {
        raw.push_back(0); /* filter: none */
        for (int x = 0; x < SIM_WIDTH; x++) {
            const uint16_t color = g_fb[y * SIM_WIDTH + x];
            raw.push_back((uint8_t)((color >> 8) & 0xF8));
            raw.push_back((uint8_t)((color >> 3) & 0xFC));
            raw.push_back((uint8_t)((color << 3) & 0xF8));
        }
    }

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    push_u32(ihdr, SIM_WIDTH);
    push_u32(ihdr, SIM_HEIGHT);
    ihdr.push_back(8);  /* bit depth */
    ihdr.push_back(2);  /* color type: truecolor */
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    push_chunk(png, "IHDR", ihdr);
    push_chunk(png, "IDAT", zlib_store(raw));
    push_chunk(png, "IEND", {});

    FILE *file = std::fopen(path, "wb");
    if (file) {
        std::fwrite(png.data(), 1, png.size(), file);
        std::fclose(file);
        std::printf("  png: %s\n", path);
    } else {
        std::printf("  png: %s 写入失败\n", path);
    }
}
