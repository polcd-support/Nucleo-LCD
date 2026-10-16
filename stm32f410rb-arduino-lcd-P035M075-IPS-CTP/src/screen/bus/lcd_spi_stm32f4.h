/*
 * LCD 像素总线（STM32F4）：SPI1 + DMA2_Stream3/Channel3。
 *
 * 与移植前的 RA4M1（SPI0 + DTC）后端是同一套契约（名字从 ScreenSpiDma_* 改成 LcdSpi_*）：Begin(hz, mode) 打开总线、
 * WriteDma() 同步搬完一块数据后才返回（返回时数据已全部移出，调用方可安全拉高 CS）。
 *
 * 硬件要点：
 *  - SPI1 只发不收回（转接板 D12/MISO 未接），用 1LINE 模式，8 位帧、MSB first，
 *    与 STM32 参考工程的逐字节数据流一致。
 *  - 时钟由 APB2 分频得到，规则是“取不超过请求值的最快档”：APB2 = 80 MHz 时
 *    请求 10 MHz -> /8 = 10.0 MHz、请求 24 MHz -> /4 = 20.0 MHz。
 *    实际值由 LcdSpi_ActualHz() 上报（串口打印 actual=）。
 *  - CS/DC/RST 由 screen 层用 digitalWrite 直接控制，这里只管 SCK/MOSI。
 *
 * 引脚复用、DMA 与 NVIC 在 Core/Src/stm32f4xx_hal_msp.c。
 */
#ifndef SCREEN_BUS_LCD_SPI_STM32F4_H
#define SCREEN_BUS_LCD_SPI_STM32F4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 打开并配置总线；mode 为 0..3（CPOL/CPHA）。失败返回 false，调用方退回逐字节路径。 */
bool LcdSpi_Begin(uint32_t hz, uint8_t mode);

/* 重新设置时钟/模式（仅在变化时真正写寄存器）。 */
bool LcdSpi_Configure(uint32_t hz, uint8_t mode);

/* DMA 发送，同步返回；length 可以大于 DMA 单次长度限制。 */
bool LcdSpi_WriteDma(const uint8_t *data, uint32_t length);

/* 阻塞式逐字节发送（DMA 不可用时的保守回退路径）。 */
bool LcdSpi_WriteBlocking(const uint8_t *data, uint32_t length);

/* 最近一次错误码，0 = 无错误。 */
int32_t LcdSpi_LastError(void);

/* 分频后的真实 SPI 时钟（Hz）。 */
uint32_t LcdSpi_ActualHz(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_BUS_LCD_SPI_STM32F4_H */
