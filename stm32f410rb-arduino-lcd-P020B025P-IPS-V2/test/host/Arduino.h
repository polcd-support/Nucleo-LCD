/*
 * 主机端 Arduino 桩件：只实现 screen/ 与 app/ 用到的部分。
 * 目的：在没有硬件的情况下把整个演示程序跑一遍，把 SPI 字节流喂给虚拟面板，
 * 检查初始化时序、像素输出与状态机画面。见 test/host/main.cpp。
 */
#ifndef HOST_ARDUINO_H
#define HOST_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <cstring>

typedef bool boolean;
typedef uint8_t byte;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define HEX 16
#define DEC 10

#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define F(x) (x)
#define PROGMEM

/* 虚拟时钟，由 test 控制。 */
uint32_t millis(void);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
void analogWrite(uint8_t pin, int value);

class HostSerial {
public:
    void begin(unsigned long) {}
    void print(const char *text)
    {
        std::fputs(text, stdout);
        std::strncat(log_, text, sizeof(log_) - std::strlen(log_) - 1);
    }
    void print(char value) { print_char(value); }
    void print(int value) { print_number((long)value); }
    void print(unsigned int value) { print_number((long)value); }
    void print(unsigned long value) { print_number((long)value); }
    void print(long value) { print_number(value); }
    void print(int value, int base) { print_base((unsigned long)value, base); }
    void print(unsigned long value, int base) { print_base(value, base); }
    void println(void) { print("\n"); }
    void println(const char *text) { print(text); print("\n"); }
    void println(int value, int base) { print_base((unsigned long)value, base); print("\n"); }
    const char *log(void) const { return log_; }
    void clear(void) { log_[0] = '\0'; }

private:
    void print_char(char c) { char b[2] = {c, 0}; print(b); }
    void print_number(long value)
    {
        char b[24];
        std::snprintf(b, sizeof(b), "%ld", value);
        print(b);
    }
    void print_base(unsigned long value, int base)
    {
        char b[24];
        if (base == 16) {
            std::snprintf(b, sizeof(b), "%lX", value);
        } else {
            std::snprintf(b, sizeof(b), "%lu", value);
        }
        print(b);
    }
    char log_[512] = {0};
};

extern HostSerial Serial;

#endif
