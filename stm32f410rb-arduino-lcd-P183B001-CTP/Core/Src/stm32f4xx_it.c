/*
 * 中断入口。
 *
 * 只保留本工程用到的：
 *   - SysTick  ：HAL 的 1 ms 时基（millis/delay）
 *   - DMA2_Stream3：SPI1 发送完成（LCD 像素搬运）
 *   - SPI1     ：HAL 发送收尾（最后一个字节的 TXE/错误处理）
 */
#include "main.h"

void NMI_Handler(void) {}
void HardFault_Handler(void)
{
    for (;;) {
    }
}
void MemManage_Handler(void)
{
    for (;;) {
    }
}
void BusFault_Handler(void)
{
    for (;;) {
    }
}
void UsageFault_Handler(void)
{
    for (;;) {
    }
}
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_lcd_spi_tx);
}

void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hLcdSpi);
}
