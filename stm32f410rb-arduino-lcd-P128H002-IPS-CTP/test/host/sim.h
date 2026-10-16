/* 主机端虚拟 GC9A01 + 引脚/命令时序记录。 */
#ifndef HOST_SIM_H
#define HOST_SIM_H

#include <stddef.h>
#include <stdint.h>

#define SIM_WIDTH 240
#define SIM_HEIGHT 240
#define SIM_MAX_COMMANDS 64

/* 本面板 0x3A = 0x05（16-bit/pixel），一个像素在 SPI 上是 2 字节大端 RGB565。 */
#define SIM_PIXEL_BYTES 2

struct SimCommandRecord {
    uint32_t time_ms;
    uint8_t command;
    uint8_t length;
    uint8_t data[16];
};

void Sim_Reset(void);

/* 由引脚/SPI 桩件调用。 */
void Sim_PinMode(uint8_t pin, uint8_t mode);
void Sim_PinWrite(uint8_t pin, uint8_t value);
void Sim_AnalogWrite(uint8_t pin, int value);
void Sim_SpiByte(uint8_t value);

/* 观察结果。 */
uint16_t Sim_Pixel(int x, int y);
int Sim_PixelCount(void);
void Sim_ResetPixelCounter(void);
int Sim_CommandCount(void);
const SimCommandRecord *Sim_Command(int index);
uint32_t Sim_FirstSpiMs(void);
int Sim_BacklightLevel(void);
int Sim_BacklightWriteCount(void);
int Sim_CurrentX(void);
int Sim_CurrentY(void);
void Sim_WritePng(const char *path);

#endif
