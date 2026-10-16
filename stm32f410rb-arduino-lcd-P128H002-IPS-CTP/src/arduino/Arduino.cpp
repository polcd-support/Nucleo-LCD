/*
 * Arduino API 兼容层的实现：数字脚、时基、调试串口。
 *
 * 引脚编号 -> STM32 引脚：与 Nucleo-64 的 Arduino Uno V3 排针位置一致
 * （依据 MB1136 / UM1724 的排针定义，Zephyr 的 nucleo_f410rb 板级 dts 亦为此映射）：
 *
 *   D0  PA3    D1  PA2    D2  PA10   D3  PB3    D4  PB5    D5  PB4    D6  PB10
 *   D7  PA8    D8  PA9    D9  PC7    D10 PB6    D11 PA7    D12 PA6    D13 PA5
 *   D14 PB9（UNO R3 的 SDA 位）      D15 PB8（UNO R3 的 SCL 位）
 *
 * 转接板只用到 D4/D5/D6/D8/D9/D10/D11/D13 与 SDA/SCL 位；其余脚保留映射，
 * 方便以后换屏或加外设。
 */
#include "Arduino.h"

#include <string.h>

#include "main.h"

namespace {

struct PinMap {
    GPIO_TypeDef *port;
    uint16_t pin;
};

const PinMap kPins[16] = {
    {GPIOA, GPIO_PIN_3},  /* D0  */
    {GPIOA, GPIO_PIN_2},  /* D1  */
    {GPIOA, GPIO_PIN_10}, /* D2  */
    {GPIOB, GPIO_PIN_3},  /* D3  */
    {GPIOB, GPIO_PIN_5},  /* D4  */
    {GPIOB, GPIO_PIN_4},  /* D5  */
    {GPIOB, GPIO_PIN_10}, /* D6  */
    {GPIOA, GPIO_PIN_8},  /* D7  */
    {GPIOA, GPIO_PIN_9},  /* D8  */
    {GPIOC, GPIO_PIN_7},  /* D9  */
    {GPIOB, GPIO_PIN_6},  /* D10 */
    {GPIOA, GPIO_PIN_7},  /* D11 */
    {GPIOA, GPIO_PIN_6},  /* D12 */
    {GPIOA, GPIO_PIN_5},  /* D13 */
    {GPIOB, GPIO_PIN_9},  /* D14 / SDA 位 */
    {GPIOB, GPIO_PIN_8},  /* D15 / SCL 位 */
};

bool g_port_a_clock;
bool g_port_b_clock;
bool g_port_c_clock;

void enable_port_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        if (!g_port_a_clock) {
            g_port_a_clock = true;
            __HAL_RCC_GPIOA_CLK_ENABLE();
        }
    } else if (port == GPIOB) {
        if (!g_port_b_clock) {
            g_port_b_clock = true;
            __HAL_RCC_GPIOB_CLK_ENABLE();
        }
    } else if (port == GPIOC) {
        if (!g_port_c_clock) {
            g_port_c_clock = true;
            __HAL_RCC_GPIOC_CLK_ENABLE();
        }
    }
}

const PinMap *lookup_pin(uint8_t pin)
{
    if (pin > 15) {
        return nullptr;
    }
    return &kPins[pin];
}

} /* namespace */

void pinMode(uint8_t pin, uint8_t mode)
{
    const PinMap *entry = lookup_pin(pin);
    GPIO_InitTypeDef gpio = {0};

    if (entry == nullptr) {
        return;
    }
    enable_port_clock(entry->port);

    gpio.Pin = entry->pin;
    gpio.Mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    /* 输入默认高阻：转接板的 TXS0104E 靠自身上拉做方向检测，固件不允许给这些信号加下拉。 */
    gpio.Pull = (mode == INPUT_PULLUP) ? GPIO_PULLUP : GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(entry->port, &gpio);
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    const PinMap *entry = lookup_pin(pin);

    if (entry == nullptr) {
        return;
    }
    HAL_GPIO_WritePin(entry->port, entry->pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint32_t millis(void)
{
    return HAL_GetTick();
}

void delay(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ------------------------------------------------------------------ 串口 */

ArduinoSerial Serial;

void ArduinoSerial::begin(unsigned long baud)
{
    hConsoleUart.Instance = USART2;
    hConsoleUart.Init.BaudRate = baud;
    hConsoleUart.Init.WordLength = UART_WORDLENGTH_8B;
    hConsoleUart.Init.StopBits = UART_STOPBITS_1;
    hConsoleUart.Init.Parity = UART_PARITY_NONE;
    hConsoleUart.Init.Mode = UART_MODE_TX_RX;
    hConsoleUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hConsoleUart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&hConsoleUart) != HAL_OK) {
        Error_Handler();
    }
}

void ArduinoSerial::write_string(const char *text)
{
    if (text == nullptr || *text == '\0') {
        return;
    }
    HAL_UART_Transmit(&hConsoleUart, (const uint8_t *)text, (uint16_t)strlen(text), 100U);
}

void ArduinoSerial::write_number(unsigned long value, int base, bool negative)
{
    char buffer[12];
    size_t index = sizeof(buffer);

    buffer[--index] = '\0';
    if (value == 0) {
        buffer[--index] = '0';
    }
    while (value > 0) {
        const unsigned long digit = value % (unsigned long)base;
        buffer[--index] = (char)(digit < 10 ? ('0' + digit) : ('A' + (digit - 10)));
        value /= (unsigned long)base;
    }
    if (negative) {
        buffer[--index] = '-';
    }
    write_string(&buffer[index]);
}

void ArduinoSerial::print(const char *text) { write_string(text); }
void ArduinoSerial::print(char value)
{
    const char text[2] = {value, '\0'};
    write_string(text);
}
void ArduinoSerial::print(int value) { write_number((unsigned long)(value < 0 ? -value : value), 10, value < 0); }
void ArduinoSerial::print(unsigned int value) { write_number(value, 10, false); }
void ArduinoSerial::print(long value) { write_number((unsigned long)(value < 0 ? -value : value), 10, value < 0); }
void ArduinoSerial::print(unsigned long value) { write_number(value, 10, false); }
void ArduinoSerial::print(int value, int base) { write_number((unsigned long)value, base, false); }
void ArduinoSerial::print(unsigned long value, int base) { write_number(value, base, false); }
void ArduinoSerial::println(void) { write_string("\r\n"); }
void ArduinoSerial::println(const char *text)
{
    write_string(text);
    write_string("\r\n");
}
void ArduinoSerial::println(int value, int base)
{
    print(value, base);
    println();
}
