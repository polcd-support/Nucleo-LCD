/* 入口头文件（CubeMX 风格）。
 *
 * 外设句柄定义在 Core/Src/main.c，DMA 句柄定义在 Core/Src/stm32f4xx_hal_msp.c，
 * 由各自的驱动模块（LCD 总线、触摸 I2C、调试串口）在需要时初始化：
 *   - SPI1 + DMA2_Stream3  -> src/screen/bus/lcd_spi_stm32f4.cpp
 *   - I2C1                 -> src/arduino/Wire.cpp
 *   - USART2               -> src/arduino/Arduino.cpp
 * 引脚复用和 DMA/NVIC 配置集中在 Core/Src/stm32f4xx_hal_msp.c。
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

extern SPI_HandleTypeDef hLcdSpi;           /* SPI1：LCD 像素总线（只发） */
extern DMA_HandleTypeDef hdma_lcd_spi_tx;   /* DMA2_Stream3/Channel3：SPI1 TX */
extern I2C_HandleTypeDef hLcdI2c;           /* I2C1：触摸（PB8/PB9） */
extern UART_HandleTypeDef hConsoleUart;     /* USART2：ST-Link 虚拟串口，115200 */

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
