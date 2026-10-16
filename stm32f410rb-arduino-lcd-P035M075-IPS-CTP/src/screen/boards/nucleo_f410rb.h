/*
 * Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 引脚映射。
 *
 * 本工程装的屏幕：P035M075-IPS-CTP（ILI9488 320x480 IPS + FT6336U 电容触摸）。
 * ILI9488 的 4 线 SPI 只能跑 18-bit（RGB666）模式，每像素 3 字节，SPI mode 3
 * （CPOL=1/CPHA=1，见 panels/panel_p035m075_ips_ctp.cpp）。
 * 触摸的 RST/INT 都用：RST 由 FT6336U 驱动复位，INT 配成输入由轮询实现读取。
 *
 * 转接板是 UNO 排针尺寸的，插到 Nucleo 的 Arduino Uno V3 排针上；
 * 排针位置 -> STM32 引脚按 MB1136/UM1724 的 Nucleo-64 定义（与 Zephyr 的
 * nucleo_f410rb 板级 dts 一致），见 src/arduino/Arduino.cpp。
 *
 *   信号            转接板网络      UNO 排针位置   Nucleo-F410RB   说明
 *   LCD SDA(MOSI)   LCD_SDA        D11            PA7 (SPI1_MOSI) U1 TXB0108 通道 A2/B2
 *   LCD SCK         LCD_CLK        D13            PA5 (SPI1_SCK)  U1 通道 A1/B1
 *   LCD CS          LCD_CS         D10            PB6             软件片选
 *   LCD DC/RS       LCD_DC         D9             PC7             软件数据/命令
 *   LCD RESET       LCD_RST        D8             PA9             软件复位（PA9 不用 USART1）
 *   背光控制        LCD_PWR        D6             PB10            U1 通道 A6/B6
 *   D7              -              D7             PA8             转接板未连接
 *   触摸 SDA        TP_SDA         SDA 位         PB9 (I2C1_SDA)  U2 TXS0104E 通道 A2/B2
 *   触摸 SCL        TP_CLK         SCL 位         PB8 (I2C1_SCL)  U2 通道 A1/B1
 *   触摸 INT        TP_INT         D4             PB5             配成输入，本驱动轮询读取
 *   触摸 RESET      TP_RST         D5             PB4             U2 通道 A4/B4，驱动复位用
 *
 * 与 UNO R4 WiFi 工程的三处差异（全部是板级差异，屏幕/触摸代码不受影响）：
 *  1. 转接板的 U1/U2 是给 5 V 主机设计的：B 侧（主机侧）接 +5 V，A 侧（面板侧）接 +3.3 V。
 *     Nucleo 的 I/O 是 3.3 V，B 侧输入门限 VIH(min) = VCCB x 0.65 = 3.25 V
 *     （TXB0108 数据手册 SCES643L 第 5 节；TXS0104E 同族），3.3 V 输出只高出约 50 mV，
 *     属于“勉强合规”。实物点不亮时先检查这里，见 README 的“已知限制”。
 *  2. UNO R4 板上的背光需要飞线强制拉高；Nucleo 这边 D6 是空闲脚，
 *     固件直接把它配成推挽输出（初始化拉低，画面准备好后拉高）。
 *  3. 触摸 I2C 走硬件 I2C1（PB8/PB9），不是软件模拟：转接板把触摸 I2C 引到
 *     UNO R3 的 SDA/SCL 位上，而 Nucleo-64 的这两个位置正好是 I2C1 的默认脚。
 */
#ifndef SCREEN_BOARD_NUCLEO_F410RB_H
#define SCREEN_BOARD_NUCLEO_F410RB_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#define SCREEN_SPI_BUS SPI
#define SCREEN_I2C_BUS Wire

#define SCREEN_PIN_CS 10
#define SCREEN_PIN_DC 9
#define SCREEN_PIN_RST 8
#define SCREEN_PIN_BL 6
#define SCREEN_PIN_TOUCH_RST 5
#define SCREEN_PIN_TOUCH_INT 4

/* 背光脚 PB10 在 F410 上没有可用的定时器通道，只能开/关；需要调光再另想办法。 */
#define SCREEN_BL_PWM 0

/* APB2 = 80 MHz，SPI1 取不超过请求值的最快分频：
 * 面板 24 MHz 请求 -> /2 = 40 MHz 太快，取 /4 = 实际 20.0 MHz。
 * 面板本身的 spi_hz 在 Panel_P035M075_IPS_CTP.spi_hz 里，实际请求值取两者较小者。 */
#define SCREEN_SPI_HZ 24000000UL

/* SPI1 + DMA2_Stream3 硬件搬运像素，保持 8 位帧和原始字节顺序。 */
#define SCREEN_SPI_DMA 1

/* DMA 不可用或出错时的保守回退路径（逐字节阻塞发送）。 */
#define SCREEN_SPI_BYTE_TRANSFER 1

/* 触摸每 10 ms 只读 5 字节；U2 的 10 k 上拉下 100 kHz 足够稳。 */
#define SCREEN_I2C_HZ 100000UL

#endif
