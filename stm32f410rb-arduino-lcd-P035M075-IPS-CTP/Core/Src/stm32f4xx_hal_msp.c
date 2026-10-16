/*
 * 外设的底层初始化（引脚复用、时钟、DMA、NVIC）。
 * 转接板上的全部信号都经过排针固定，这里就是“所有引脚唯一落点”。
 *
 *   LCD（U1 TXB0108，B 侧 5 V 参考，A 侧 3.3 V 到面板）
 *     D13 -> PA5  SPI1_SCK   (AF5)
 *     D11 -> PA7  SPI1_MOSI  (AF5，只发；D12/PA6 转接板未接，不配置)
 *     D10 -> PB6  软件 CS    (普通输出，由 src/arduino/Arduino.cpp 的 pinMode 配置)
 *     D9  -> PC7  软件 DC/RS (同上)
 *     D8  -> PA9  软件 RST   (同上，PA9 不用 USART1，调试口走 USART2)
 *     D6  -> PB10 背光使能   (同上)
 *   触摸（U2 TXS0104E，B 侧上拉到 +5 V）
 *     SDA 位 -> PB9  I2C1_SDA (AF4)
 *     SCL 位 -> PB8  I2C1_SCL (AF4)
 *     D5/D4 -> PB4/PB5 复位/中断（普通输出/输入）
 *   调试串口：ST-Link 虚拟串口接 USART2，PA2/PA3（AF7）
 */
#include "main.h"

DMA_HandleTypeDef hdma_lcd_spi_tx;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};

    if (hspi->Instance != SPI1) {
        return;
    }

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_7; /* D13 SCK、D11 MOSI */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* SPI1_TX 固定走 DMA2_Stream3/Channel3（与 ST 官方 F4 例程一致）。 */
    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_lcd_spi_tx.Instance = DMA2_Stream3;
    hdma_lcd_spi_tx.Init.Channel = DMA_CHANNEL_3;
    hdma_lcd_spi_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_lcd_spi_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_lcd_spi_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_lcd_spi_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_lcd_spi_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_lcd_spi_tx.Init.Mode = DMA_NORMAL;
    hdma_lcd_spi_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_lcd_spi_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_lcd_spi_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(hspi, hdmatx, hdma_lcd_spi_tx);

    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(SPI1_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio = {0};

    if (hi2c->Instance != I2C1) {
        return;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* 转接板 U2 的 B 侧已有 10 k 上拉（拉到 +5 V），此处保持开漏、不开内部上拉。 */
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9; /* SCL/SDA，即 UNO R3 排针的 SCL/SDA 位 */
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART2) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3; /* PA2 TX / PA3 RX，接 ST-Link 虚拟串口 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
}
