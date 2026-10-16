# stm32f410rb-arduino-lcd-P0147B0108

Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 + P0147B0108
（ST7789P3 BOE 1.47 寸 172x320 IPS，**无触摸**）的点屏演示。

工程由 `arduino-uno-r4-wifi/P0147B0108` 移植而来：效果、屏幕层分层、初始化寄存器表
全部保持一致，只把“板卡 + 总线后端”换成 Nucleo-F410RB（Arduino Uno V3 排针 + STM32 HAL）。

```
彩条 3s -> 灰阶 3s -> 彩条 3s -> ...
```

起始画面为彩条，与 STM32 参考工程 `stm32g0b1rc-P0147B0108` 的 `LCD_Display_RunTest()`
一致：本模组无触摸，参考工程也不绘制 Clear 按钮，因此没有手写/清屏画面。
以上流程已用主机端虚拟面板跑通并逐帧核对（见 `test/`）。

## 硬件与接线

转接板是 UNO 排针尺寸的，直接插在 Nucleo 的 Arduino Uno V3 排针上，不需要外接飞线。
UNO 排针位置 -> STM32 引脚的对应关系按 MB1136/UM1724 的 Nucleo-64 定义
（与 Zephyr 的 `nucleo_f410rb` 板级 dts 一致）：

| 信号 | 转接板网络 | UNO 排针位置 | Nucleo-F410RB | 说明 |
|---|---|---|---|---|
| LCD SDA/MOSI | LCD_SDA | D11 | PA7 | SPI1_MOSI，经 U1 TXB0108 通道 A2/B2 |
| LCD SCK | LCD_CLK | D13 | PA5 | SPI1_SCK，经 U1 通道 A1/B1 |
| LCD CS | LCD_CS | D10 | PB6 | 软件片选（U1 通道 A3/B3） |
| LCD DC/RS | LCD_DC | D9 | PC7 | 软件数据/命令（U1 通道 A4/B4） |
| LCD RST | LCD_RST | D8 | PA9 | 软件复位（U1 通道 A5/B5；PA9 不用 USART1） |
| 背光控制 | LCD_PWR | D6 | PB10 | 固件推挽输出：初始化拉低，画面准备好后拉高 |
| — | — | D7 | PA8 | 转接板未连接 |
| 触摸 SDA/SCL/RST/INT | TP_SDA/TP_CLK/TP_INT/TP_RST | — | PB9/PB8/PB5/PB4 | **本模组无触摸**，`SCREEN_PIN_TOUCH_RST` / `SCREEN_PIN_TOUCH_INT` 为 -1，I2C 不接任何东西 |
| 调试串口 | — | D0/D1 | PA3/PA2 | USART2，接 ST-Link 虚拟串口，115200 8N1 |

转接板 U1/U2 的 OE 固定接 3.3 V，电平转换器始终使能，固件不控制它。
底板的 3V3/5V/GND 由 J2 从 Nucleo 的 Arduino 电源排针取电。

固件顺序是 **LCD 控制脚置为确定电平（复位拉低、CS 拉高）→ `SPI.begin()`/`Wire.begin()`
→ 面板复位与初始化 → 绘制首屏**，`src/screen/screen.cpp` 严格按这个顺序执行，测试会校验。

### 与 UNO R4 工程的板级差异

1. **电平参考点**：转接板的 B 侧（主机侧）按 5 V 主机设计（VCCB = +5 V），Nucleo 的
   I/O 是 3.3 V。TXB0108 数据手册（SCES643L 第 5 节）规定的输入高门限是
   `VIH(min) = VCCI x 0.65`，即 B 侧 3.25 V；3.3 V 输出只高出约 50 mV，属于“勉强合规”。
   点亮失败时先查这里，详见“已知限制”。
2. **背光**：UNO R4 板上 D6 走不通，实物用飞线把背光强制拉高；Nucleo 这边 D6（PB10）是
   空闲脚，固件直接驱动它（`SCREEN_PIN_BL = 6`）。如果转接板上还留着那根飞线，也不冲突。
3. **触摸总线**：转接板把触摸 I2C 引到 UNO R3 的 SDA/SCL 位上，而 Nucleo-64 的这两个位置
   正好是 I2C1 的默认引脚（PB9/PB8），所以基础设施里用**硬件 I2C**，不是软件模拟。
   本模组无触摸，这条路径没有接任何设备，保留给同族带触摸的模组。

## 总线与时钟

| 项 | 配置 |
|---|---|
| SYSCLK | 80 MHz（HSI 16 MHz -> PLL /16 x160 /2） |
| APB1 / APB2 | 40 MHz / 80 MHz |
| LCD SPI | SPI1，只发（1LINE），8 位帧、MSB first，模式 0，请求 24 MHz，APB2/4 = **20.0 MHz** |
| 像素搬运 | DMA2_Stream3 / Channel3（SPI1_TX），每块完成后才返回 |
| 触摸 I2C | I2C1，100 kHz 标准模式（本模组无触摸，未接设备） |
| 调试串口 | USART2 115200（ST-Link 虚拟串口） |
| Flash / RAM | 2 wait states；Code 14236 B + RO 1172 B，ZI 7804 B |

请求值与面板侧上限一致（`Panel_P0147B0108.spi_hz = SCREEN_SPI_HZ = 24000000UL`），
80 MHz 的 APB2 向下取 **/4 = 20.0 MHz**（/2 = 40 MHz 超过面板上限）。
UNO R4 那边 RA4M1 的 48 MHz 外设时钟能跑到 24 MHz，两条线的像素时钟因此不同，
但都低于 ST7789P3 4 线写时序的上限。

## 编译与烧录

用 Keil uVision5（本机 MDK Plus 5.41 + Arm Compiler 6.22 + Keil.STM32F4xx_DFP 3.0.0）：

1. 打开 `MDK-ARM/stm32f410rb-arduino-lcd-P0147B0108.uvprojx`；
2. F7 编译（0 error / 0 warning）；
3. 调试器已配成 ST-Link（SWD，10 MHz），F8 下载并复位运行；
4. 串口助手打开 ST-Link 的虚拟串口，115200。

命令行等价写法：

```powershell
& "F:\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\stm32f410rb-arduino-lcd-P0147B0108.uvprojx" -o build.log
```

产物在 `MDK-ARM/stm32f410rb-arduino-lcd-P0147B0108/`：`*.axf`（调试）、`*.hex`（可烧）、`*.map`。
用 OpenOCD 烧 hex：

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program MDK-ARM/stm32f410rb-arduino-lcd-P0147B0108/stm32f410rb-arduino-lcd-P0147B0108.hex verify reset exit"
```

`Core/` 下的 HAL 取自 `STM32Cube_FW_F4_V1.28.3`（BSD-3-Clause，见
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`），只保留了本工程实际编译的模块
（见 `Core/Inc/stm32f4xx_hal_conf.h`）。本次没有用 CubeMX 生成工程，因此仓库里没有 `.ioc`。

上电自检输出（115200）：

```
screen: P0147B0108  panel=P0147B0108 (ST7789P3 172x320)  172x320  rotation=0  spi=24MHz  bpp=2  tx=DMA actual=20.0MHz  touch=none
```

`spi=24MHz` 是请求值（受 `Panel_P0147B0108.spi_hz` 和板级 `SCREEN_SPI_HZ` 限制），
`actual=20.0MHz` 是 APB2 分频后的真实时钟。`tx=DMA` 表示走 DMA2 搬运；若显示
`tx=byte(err=...)` 说明 DMA 后端没起来，屏幕仍会用逐字节回退路径工作，但慢很多。
本模组无触摸，`touch=none` 且自检不会去读芯片 ID。

## 目录结构

```
Core/                              系统时钟、HAL 配置、MSP（所有引脚复用集中在这里）、中断
Drivers/                           STM32Cube HAL + CMSIS（来自 STM32Cube_FW_F4_V1.28.3）
src/arduino/                       Arduino API 兼容层（pinMode/digitalWrite/delay/millis/Serial/SPI/Wire）
src/app/demo_app.{h,cpp}           LCD_Display_RunTest() 的状态机与各画面
src/screen/                        屏幕模块（换屏只动这里）
  screen_config.h                  选板卡、选屏幕、默认方向、缓冲区大小
  screen.{h,cpp}                   对外接口：初始化时序、SPI 像素流水线、旋转、触摸换算
  screen_device.h                  数据模型：面板 / 触摸驱动 / 设备
  screen_devices.h                 设备注册表（新增屏幕登记处）
  boards/nucleo_f410rb.h           板卡引脚与总线映射
  bus/lcd_spi_stm32f4.{h,cpp}      SPI1 + DMA2_Stream3 发送后端（含时钟分频计算）
  panels/panel_p0147b0108.{h,cpp}  ST7789P3 面板描述与初始化表
  gfx/gfx.{h,cpp}                  绘图 API（LCD_*），移植自参考工程 lcd.c
  gfx/fonts.{h,cpp}                ASCII 12/16/24/32 与中文点阵（来自参考工程 lcdfont.h）
  gfx/image.h                      RLE 图像结构（本工程未放 LOGO 资产）
test/host/                         主机端验证（Arduino 桩件 + 虚拟 ST7789P3）
test/run.sh                        编译并运行主机端验证，结果图输出到 test/out/
tools/pic2rle.py                   把 Image2Lcd 风格的 C 数组转成 RLE 资产（本工程未用）
MDK-ARM/                           Keil 工程（startup、uvprojx/uvoptx、编译产物）
```

`src/` 与 `src/arduino/` 的分工：屏幕层只认“Arduino 风格”的 API，实物上由
`src/arduino/*` 落到 HAL，主机上由 `test/host/*` 落到虚拟面板，两边编译同一份 `src/screen`。

本模组无触摸，因此没有 `src/screen/touch/`：设备表里 `touch = NULL`，
`Screen_HasTouch()` 返回 `false`，`Screen_TouchRead()` 恒返回 0。

## 分层约定

- `app` 只调用 `gfx`（`LCD_*`）和 `screen`（`Screen_*`），不碰寄存器。
- `gfx` 颜色统一 RGB565；`screen` 负责换算成面板真正需要的格式（本面板即 RGB565，2 字节/像素）。
- 面板只描述“寄存器怎么写”，方向修正（翻转/交换/偏移）写在设备里。
- 引脚复用集中在 `Core/Src/stm32f4xx_hal_msp.c`；LCD 的 CS/DC/RST/背光是普通 GPIO，
  由 `pinMode/digitalWrite` 在运行时配置。
- 一次绘制的正确姿势：

```cpp
Screen_SetWindow(x, y, x + w - 1, y + h - 1);   // 设置窗口，CS 保持有效
Screen_WritePixels565(pixels, w * h);            // 由它释放 CS
```

## 与移植前工程的差异（除板卡外）

| 项 | arduino-uno-r4-wifi/P0147B0108 | 本工程 | 原因 |
|---|---|---|---|
| 总线后端 | `src/screen/bus/spi_dma_ra4m1.*`（SPI0 + DTC） | `src/screen/bus/lcd_spi_stm32f4.*`（SPI1 + DMA2） | 换芯片，接口契约（Begin/Write/LastError/ActualHz）不变 |
| 后端名 | `ScreenSpiDma_*` | `LcdSpi_*` | 名字不再绑定某个芯片的搬运器 |
| 后端编译开关 | `SCREEN_SPI_DMA && defined(ARDUINO_ARCH_RENESAS_UNO)` | `SCREEN_SPI_DMA && defined(STM32F410Rx)` | 主机端测试没有该宏，自动只编译逐字节回退路径 |
| SPI 时钟 | 请求 24 MHz，RA4M1 PCLKA=48 MHz 下实际 24 MHz | 请求 24 MHz，APB2=80 MHz 下实际 20 MHz | 分频器不同 |
| 自检行 | `tx=DTC ... actual=24.0MHz` | `tx=DMA ... actual=20.0MHz` | 后端与真实时钟都变了 |
| 背光 | `SCREEN_PIN_BL = -1`，实物飞线强制开启，固件不碰 D6 | `SCREEN_PIN_BL = 6`（PB10），固件初始化拉低、setup 后拉高 | 这块板子的背光由固件驱动 |
| 背光断言 | “D6 始终未被固件操作” | “初始化拉低、setup 后拉高，全程只碰 3 次” | 同上 |
| 标题文字 | 本 demo 不显示文字 | 相同（无 `UNO R4 WiFi` 这一行） | 参考工程 `LCD_Display_RunTest()` 只画彩条与灰阶 |

面板本体（初始化表、172x320、四方向 MADCTL、GRAM 偏移 34、
`pixel_format = RGB565`、`spi_hz = 24000000UL`、mode 0）与 UNO R4 工程逐字一致，
`src/screen/gfx/`、`src/app/demo_app.cpp`、`tools/pic2rle.py`、`test/host/` 同样照搬，
差异只在上表列出的板级/测试项。

## 验证

### 已执行

1. 主机端行为验证（`bash ./test/run.sh`，g++ 编译 `src/screen` + `src/app` 并喂给虚拟 ST7789P3）：
   **全部通过（失败 0 项）**，覆盖
   - 初始化命令序列（20 条）、各命令数据长度、首条 `0x36 = 0x00`、`0x3A = 0x05`（RGB565）、
     `0x11` 后 120 ms、`0x29` 后 20 ms 延时；
   - 背光脚 D6：初始化只碰 2 次（pinMode + 拉低，背光为 0），setup 后拉高（255）且只额外写一次，
     整个演示期间无多余操作；
   - 无触摸：`Screen_HasTouch() == false`、串口日志 `touch=none`、`bpp=2`；
   - 竖屏 GRAM 列偏移：清屏的 `0x2A = 34..205`、`0x2B = 0..319`，可见第 0 列落在 GRAM 第 34 列，
     GRAM 0..33 列未被写（居中留边）；
   - setup 清屏写满 172x320 = 55040 像素；
   - 6 条彩条的颜色与 6 个分界 x（0..27 / 28..56 / 57..85 / 86..113 / 114..142 / 143..171）；
   - 灰阶 24 级首格黑、末格白、逐格递增、余数 4 像素补在最后一格；
   - 起始状态为彩条，3 s 后切灰阶、6 s 后切回彩条。

   结果图 `test/out/*.png` 已人工核对：`1-colorbars.png`（彩条）、`2-grayscale.png`（灰阶）、
   `3-colorbars-again.png`（切回彩条）。
2. Keil 实机工程编译（Arm Compiler 6.22）：
   `Program Size: Code=14236 RO-data=1172 RW-data=52 ZI-data=7804`，0 error / 0 warning，
   已生成 `*.axf` 与 `*.hex`。

### 需要实物确认（未做）

- 20 MHz SPI 的波形裕量：若出现随机色点、错行或偶发花屏，把 `SCREEN_SPI_HZ` 改回
  `10000000UL`（APB2/8，面板侧上限仍是 24 MHz）再测；
- 面板实际显色与 Gamma、GRAM 偏移 34 在实物上的观感（172 在 240 列里居中）；
- 背光电流与亮度（PB10 只能开/关）；
- 主机端测试走的是逐字节回退路径（`tx=byte`），DMA 路径只在实物上运行，
  串口必须打印 `tx=DMA`；
- **3.3 V 主机侧电平裕量**：D13/D11/D10/D9/D8/D6 都要经过 5 V 参考的电平转换器，
  若出现花屏、不亮、随机色点，先量 D13 的高电平（应接近 3.3 V），
  再按“已知限制”处理；
- 触摸路径（I2C1/PB8/PB9）：本模组无触摸，未接设备，也没有实物验证过。

## 已知限制

- **电平门限余量只有 50 mV**：转接板 U1/U2 的 B 侧接 +5 V，输入高门限 3.25 V（TXB0108
  SCES643L 第 5 节），而 Nucleo 的 3.3 V 输出只高出 50 mV。这是硬件决定的，固件无法改善；
  如果实测点不亮，可选的处理顺序是：
  1. 用示波器确认 D13/D11 高电平是否到 3.3 V（排线/焊点接触不良更常见）；
  2. 把 U1/U2 的 VCCB 从 +5 V 改接到 +3.3 V（需要割线飞线，改完 B 侧门限降到 2.15 V）；
  3. 换用原生 3.3 V 的转接板/排线。
- **触摸总线的 B 侧被 10 k 上拉到 +5 V**：如果换用带触摸的同族模组，I2C1 用的 PB8/PB9
  必须能承受 5 V。F410 数据手册里 I2C 引脚一般标为 5 V 容忍（FT），但本仓库没有随附
  数据手册、也未实测，请实物核对一次；不确定时把 U2 的 VCCB 改接 +3.3 V。
- **本模组无触摸**：没有手写/清屏画面，`Screen_TouchRead()` 恒返回 0，
  也不能靠软件造点；需要触摸时从同族带触摸的模组拷回 `src/screen/touch/` 并改
  `screen_config.h` 的 `SCREEN_TOUCH_DRIVER` 与板级触摸引脚。
- 背光只能开/关：PB10 在 F410 上没有可用的定时器通道，`SCREEN_BL_PWM` 为 0，
  `Screen_BacklightLevel()` 只按 0/255 处理。亮度由转接板 5 V + 面板背光电路决定，
  不代表规格书亮度。
- 整屏填充 172x320 需要发 110 080 字节，20 MHz 下纯线路传输下限约 44 ms，
  纯色/灰阶画面上能看到自上而下的推进。
- 本 demo 每 3 s 切换一次画面，切换时整屏重绘。
- 字符宽度按 `sizey/2`，超出屏幕的字模整字丢弃（不会写出窗口外）。

## 资产来源

- `Drivers/`：STM32Cube_FW_F4_V1.28.3（BSD-3-Clause）；
- `src/screen/gfx/fonts.{h,cpp}`：参考工程 `HARDWARE/LCD/lcdfont.h`，只把中文索引从 GBK
  改成 UTF-8 三字节，点阵数据未动（本 demo 未使用，绘图库能力保留）；
- `src/screen`（面板/设备/注册表/gfx）、`src/app`、`test/host`、`tools/pic2rle.py`：
  移植自 `arduino-uno-r4-wifi/P0147B0108`（其自身又来自 `stm32g0b1rc-P0147B0108`）；
- `Core/`、`src/arduino/`、`src/screen/{screen.*,screen_device.h,bus/*,boards/*}`：
  取自本仓的 F410 样板 `stm32f410rb-arduino-lcd`（P024C135），保证基础设施两条线一致；
- `MDK-ARM/startup_stm32f410rx.s`、工程骨架：STM32Cube_FW_F4 的 Nucleo-F410RB 模板。

本工程 demo 不显示 LOGO，因此没有 `src/screen/assets/`。需要加图像资产时用
`tools/pic2rle.py`：本族 `pic.h` 的 `gImage_logo` 是 RGB565（数组长度 = 8 + W*H*2），
该工具支持 `--bpp 3`（RGB888）与 `--bpp 2`（RGB565）。
