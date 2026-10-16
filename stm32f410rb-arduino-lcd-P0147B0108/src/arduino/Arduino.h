/*
 * 极简 Arduino API 兼容层（只实现 src/ 下真正用到的部分）。
 *
 * 为什么需要它：src/screen、src/app 是从 arduino-uno-r4-wifi 的对应屏幕工程原样移植过来的，
 * 那一层本来就只依赖 pinMode/digitalWrite/delay/millis/Serial/SPI/Wire 这一小撮 API。
 * 这里把它们落到 STM32F4 HAL 上，移植过来的屏幕代码一行都不用改写，
 * test/host 里的主机端桩件也仍然是同一套接口（见 test/host/Arduino.h）。
 *
 * 数字脚编号与 UNO R3 排针位置一一对应（D0..D15），映射表见 Arduino.cpp 与
 * src/screen/boards/nucleo_f410rb.h。
 */
#ifndef ARDUINO_COMPAT_H
#define ARDUINO_COMPAT_H

#include <stddef.h>
#include <stdint.h>

typedef bool boolean;
typedef uint8_t byte;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define DEC 10
#define HEX 16

/* 本工程不使用 AVR 的 Flash 空间分区；F() 只为了和移植前的代码保持一致。 */
#define F(text) (text)

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
uint32_t millis(void);
void delay(uint32_t ms);

/* 与 Arduino 的 Serial 用法相同：begin/print/println，只保留本工程用到的重载。 */
class ArduinoSerial {
public:
    void begin(unsigned long baud);
    void print(const char *text);
    void print(char value);
    void print(int value);
    void print(unsigned int value);
    void print(long value);
    void print(unsigned long value);
    void print(int value, int base);
    void print(unsigned long value, int base);
    void println(void);
    void println(const char *text);
    void println(int value, int base);

private:
    void write_string(const char *text);
    void write_number(unsigned long value, int base, bool negative);
};

extern ArduinoSerial Serial;

#endif /* ARDUINO_COMPAT_H */
