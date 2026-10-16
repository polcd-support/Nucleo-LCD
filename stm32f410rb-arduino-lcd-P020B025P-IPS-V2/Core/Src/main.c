/*
 * Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 的点屏与触摸示例。
 * 本工程装的屏幕由 src/screen/screen_config.h 选择（本族每块屏一个工程目录）。
 *
 * 本文件只负责系统时钟和主循环，接线/屏幕逻辑在 src/ 下：
 *   src/screen/boards/nucleo_f410rb.h   板卡引脚映射
 *   src/screen/screen.{h,cpp}           屏幕层（窗口、旋转、触摸换算）
 *   src/screen/panels/panel_<屏>.{h,cpp}  该面板的初始化表
 *   src/app/demo_app.*                  与 STM32 参考工程 LCD_DEMO() 一致的状态机
 *
 * 时钟：HSI 16 MHz -> PLL -> SYSCLK 80 MHz，APB1 40 MHz，APB2 80 MHz。
 * SPI1 从 APB2 分频，规则是“取不超过面板请求值的最快档”：
 * 请求 10 MHz -> /8 = 10.0 MHz（ILI9341），请求 24 MHz -> /4 = 20.0 MHz（ST7789/GC9A01）。
 * 请求值 = min(面板 Panel_*.spi_hz, 板级 SCREEN_SPI_HZ)，真实时钟见串口打印的 actual=。
 */
#include "main.h"

#include "demo_app.h"

SPI_HandleTypeDef hLcdSpi;
I2C_HandleTypeDef hLcdI2c;
UART_HandleTypeDef hConsoleUart;

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    /* 屏幕、触摸、调试串口的初始化都在这里完成（逐级调用 Screen_Init / 触摸驱动）。 */
    DemoApp_Setup();

    for (;;) {
        DemoApp_Loop();
    }
}

/*
 * System Clock Configuration
 *   SYSCLK  = 80 MHz  (PLL: HSI 16 MHz / 16 * 160 / 2)
 *   HCLK    = 80 MHz  (AHB  /1)
 *   PCLK1   = 40 MHz  (APB1 /2，上限 50 MHz)
 *   PCLK2   = 80 MHz  (APB2 /1，上限 100 MHz)
 *   VCO     = 160 MHz (100..432 MHz)
 *   Flash   = 2 wait states（60..90 MHz 档）
 *
 * APB2 = 80 MHz 是为了让 SPI1 的分频档（/4 = 20 MHz、/8 = 10 MHz）刚好覆盖
 * 本族面板的 24 MHz 与 10 MHz 请求，实际时钟以串口打印的 actual= 为准。
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 16;
    osc.PLL.PLLN = 160;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 4; /* 本工程不使用 USB/SDIO，仅满足 HAL 写入 PLLCFGR 的需要 */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}
