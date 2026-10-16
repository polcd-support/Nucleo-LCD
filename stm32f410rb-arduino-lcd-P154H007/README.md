# stm32f410rb-arduino-lcd-P154H007

Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 + P154H007
（ST7789P3 240x240 IPS，**无触摸**）的点屏演示。

工程由 `arduino-uno-r4-wifi/P154H007` 移植而来：面板初始化寄存器表、宽高、
四方向 `madctl`、GRAM 偏移、像素格式、SPI 时钟/模式、LOGO 资产、演示画面与文字
全部保持一致，只把“板卡 + 总线后端”换成 Nucleo-F410RB（Arduino Uno V3 排针 + STM32 HAL）。

```
LOGO 3s -> 多字号文字 2s -> 全屏纯色 5 色 x 1s -> 彩条 3s -> 灰阶 3s -> 回到 LOGO
```

与 STM32 参考工程 `stm32g0b1rc-P154H007` 的 `LCD_DEMO()` 一致，整段序列无限循环；
本工程把这段序列做成循环状态机（进入状态时画一次），画面观感不变。

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
| 触摸 SDA | TP_SDA | SDA 位 | PB9 | 本模组无触摸，不接触摸器件（I2C1 仍按通用流程打开，见下） |
| 触摸 SCL | TP_CLK | SCL 位 | PB8 | 同上 |
| 触摸 INT | TP_INT | D4 | PB5 | **不配置**（`SCREEN_PIN_TOUCH_INT = -1`） |
| 触摸 RESET | TP_RST | D5 | PB4 | **不配置**（`SCREEN_PIN_TOUCH_RST = -1`） |
| 调试串口 | — | D0/D1 | PA3/PA2 | USART2，接 ST-Link 虚拟串口，115200 8N1 |

P154H007 模组没有触摸，转接板上的 TP_INT/TP_RST 在本工程不配置；`Wire.begin()` /
`setClock()` 仍会按 `screen.cpp` 的通用流程调用一次（PB8/PB9 配成 I2C1 复用开漏），
但没有任何从机器件、也不发起读写，`Screen_HasTouch()` 恒为 `false`。

转接板 U1/U2 的 OE 固定接 3.3 V，电平转换器始终使能，固件不控制它。
底板的 3V3/5V/GND 由 J2 从 Nucleo 的 Arduino 电源排针取电。

### 与 UNO R4 工程的板级差异

1. **电平参考点**：转接板的 B 侧（主机侧）按 5 V 主机设计（VCCB = +5 V），Nucleo 的
   I/O 是 3.3 V。TXB0108 数据手册（SCES643L 第 5 节）规定的输入高门限是
   `VIH(min) = VCCB x 0.65`，即 3.25 V；3.3 V 输出只高出约 50 mV，属于“勉强合规”。
   点不亮时先查这里，详见“已知限制”。
2. **背光**：UNO R4 板上 D6 走不通，实物用飞线把背光强制拉高（那边 `SCREEN_PIN_BL = -1`）；
   Nucleo 这边 D6（PB10）是空闲脚，固件直接驱动它（`SCREEN_PIN_BL = 6`）。
   如果转接板上还留着那根飞线，也不冲突。
3. **触摸总线**：本模组无触摸，F410 侧沿用样板的 I2C1 初始化（PB8/PB9），
   不像 UNO R4 工程那样完全不配置总线；两者都不与触摸器件通信。

## 总线与时钟

| 项 | 配置 |
|---|---|
| SYSCLK | 80 MHz（HSI 16 MHz -> PLL /16 x160 /2） |
| APB1 / APB2 | 40 MHz / 80 MHz |
| LCD SPI | SPI1，只发（1LINE），8 位帧、MSB first、mode 0（CPOL=0/CPHA=0） |
| LCD SPI 时钟 | 请求 24 MHz（面板上限），APB2 = 80 MHz 分频档为 40/20/10 MHz，取不超过请求值的 /4 = **20.0 MHz** |
| 像素搬运 | DMA2_Stream3 / Channel3（SPI1_TX），每块完成后才返回 |
| 触摸 I2C | I2C1，100 kHz 标准模式（本屏无触摸，仅初始化占位） |
| 调试串口 | USART2 115200（ST-Link 虚拟串口） |
| Flash / RAM | 2 wait states；Code 17864 B + RO 29912 B，RW 52 B + ZI 13692 B |

选择 80 MHz 而不是 F410 的上限 100 MHz，是为了和样板共用同一套时钟配置
（APB2 的分频档在 80 MHz 下同样覆盖本面板需要的 20 MHz 档）。

## 编译与烧录

用 Keil uVision5（本机 MDK Plus 5.41 + Arm Compiler 6.22 + Keil.STM32F4xx_DFP 3.0.0）：

1. 打开 `MDK-ARM/stm32f410rb-arduino-lcd-P154H007.uvprojx`；
2. F7 编译（0 error / 0 warning）；
3. 调试器已配成 ST-Link（SWD，10 MHz），F8 下载并复位运行；
4. 串口助手打开 ST-Link 的虚拟串口，115200。

命令行等价写法：

```powershell
& "F:\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\stm32f410rb-arduino-lcd-P154H007.uvprojx" -o build.log
```

产物在 `MDK-ARM/stm32f410rb-arduino-lcd-P154H007/`：`*.axf`（调试）、`*.hex`（可烧）、`*.map`。
用 OpenOCD 烧 hex：

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program MDK-ARM/stm32f410rb-arduino-lcd-P154H007/stm32f410rb-arduino-lcd-P154H007.hex verify reset exit"
```

`Core/` 下的 HAL 取自 `STM32Cube_FW_F4_V1.28.3`（BSD-3-Clause，见
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`），只保留了本工程实际编译的模块
（见 `Core/Inc/stm32f4xx_hal_conf.h`）。本次没有用 CubeMX 生成工程，因此仓库里没有 `.ioc`。

上电自检输出（115200）：

```
screen: P154H007  panel=P154H007 (ST7789P3 240x240)  240x240  rotation=0  spi=24MHz  bpp=2  tx=DMA  actual=20.0MHz  touch=none
```

`spi=24MHz` 是请求值（取 `Panel_P154H007.spi_hz` 与板级 `SCREEN_SPI_HZ` 的较小者），
`actual=20.0MHz` 是 APB2 分频后的真实时钟。`tx=DMA` 表示走 DMA2 搬运；若显示
`tx=byte(err=...)` 说明 DMA 后端没起来，屏幕仍会用逐字节回退路径工作，但慢很多。
`touch=none` 表示该设备未配置触摸（本模组正常）。

## 目录结构

```
Core/                              系统时钟、HAL 配置、MSP（所有引脚复用集中在这里）、中断
Drivers/                           STM32Cube HAL + CMSIS（来自 STM32Cube_FW_F4_V1.28.3）
src/arduino/                       Arduino API 兼容层（pinMode/digitalWrite/delay/millis/Serial/SPI/Wire）
src/app/demo_app.{h,cpp}           LCD_DEMO() 的状态机与各画面
src/screen/                        屏幕模块（换屏只动这里）
  screen_config.h                  选板卡、选屏幕、触摸驱动、默认方向、缓冲区大小
  screen.{h,cpp}                   对外接口：初始化时序、SPI 像素流水线、旋转、触摸换算
  screen_device.h                  数据模型：面板 / 触摸驱动 / 设备
  screen_devices.h                 设备注册表（新增屏幕登记处）
  boards/nucleo_f410rb.h           板卡引脚与总线映射
  bus/lcd_spi_stm32f4.{h,cpp}      SPI1 + DMA2_Stream3 发送后端（含时钟分频计算）
  panels/panel_p154h007.{h,cpp}    ST7789P3 面板描述与初始化表
  gfx/gfx.{h,cpp}                  绘图 API（LCD_*），移植自参考工程 lcd.c
  gfx/fonts.{h,cpp}                ASCII 12/16/24/32 与中文点阵（来自参考工程 lcdfont.h）
  gfx/image.h                      RLE 图像结构
  assets/logo.h                    LOGO 资产（240x220 RGB565 RLE，3522 段）
test/host/                         主机端验证（Arduino 桩件 + 虚拟 ST7789P3）
test/run.sh                        编译并运行主机端验证，结果图输出到 test/out/
tools/pic2rle.py                   把 Image2Lcd 风格的 C 数组转成 RLE 资产
MDK-ARM/                           Keil 工程（startup、uvprojx/uvoptx、编译产物）
```

本模组无触摸，因此**没有** `src/screen/touch/` 目录，`screen_config.h` 里选中
`SCREEN_TOUCH_DRIVER_NONE`，也没有 `SCREEN_TOUCH_THRESHOLD`。
`src/` 与 `src/arduino/` 的分工：屏幕层只认“Arduino 风格”的 API，实物上由
`src/arduino/*` 落到 HAL，主机上由 `test/host/*` 落到虚拟面板，两边编译同一份 `src/screen`。

## 分层约定

- `app` 只调用 `gfx`（`LCD_*`）和 `screen`（`Screen_*`），不碰寄存器。
- `gfx` 颜色统一 RGB565；`screen` 负责换算成面板真正需要的格式（本面板即 RGB565，2 字节/像素）。
- 面板只描述“寄存器怎么写”，触摸只描述“寄存器怎么读”，方向修正（翻转/交换/偏移）写在设备里。
- 引脚复用集中在 `Core/Src/stm32f4xx_hal_msp.c`；LCD 的 CS/DC/RST/背光由
  `pinMode/digitalWrite` 在运行时配置。
- 一次绘制的正确姿势：

```cpp
Screen_SetWindow(x, y, x + w - 1, y + h - 1);   // 设置窗口，CS 保持有效
Screen_WritePixels565(pixels, w * h);            // 由它释放 CS
```

## 无触摸

P154H007 不带触摸：设备表里 `ScreenDevice::touch` 为 `NULL`，`screen_config.h` 选中
`SCREEN_TOUCH_DRIVER_NONE`，没有触摸驱动代码参与编译。因此：

- `Screen_HasTouch()` 返回 `false`，`Screen_TouchRead()` / `Screen_TouchReadRaw()` 恒返回 0；
- `boards/nucleo_f410rb.h` 里 `SCREEN_PIN_TOUCH_RST` / `SCREEN_PIN_TOUCH_INT` 为 `-1`，这两个脚不配置；
- 主机端测试断言以上行为（不做坐标换算）。

若换用带触摸的模组：在 `src/screen/touch/` 加实现 `ScreenTouchDriver` 的驱动
（FT6336U 或 CST816）、在 `screen_config.h` 里新增并选中对应的 `SCREEN_TOUCH_DRIVER_XXX`、
在板级头文件里给出触摸 RST/INT/阈值，再在 `panels/panel_p154h007.cpp` 里把
`ScreenDevice::touch` 指向该设备的 `ScreenTouchConfig`。

## 与移植前工程的差异（除板卡外）

| 项 | arduino-uno-r4-wifi/P154H007 | 本工程 | 原因 |
|---|---|---|---|
| 总线后端 | `src/screen/bus/spi_dma_ra4m1.*`（SPI0 + DTC） | `src/screen/bus/lcd_spi_stm32f4.*`（SPI1 + DMA2） | 换芯片，接口契约（Begin/Write/LastError/ActualHz）不变 |
| 后端名 | `ScreenSpiDma_*` | `LcdSpi_*` | 名字不再绑定某个芯片的搬运器 |
| 后端编译开关 | `SCREEN_SPI_DMA && defined(ARDUINO_ARCH_RENESAS_UNO)` | `SCREEN_SPI_DMA && defined(STM32F410Rx)` | 主机端测试没有该宏，自动只编译逐字节回退路径 |
| 自检行 | `tx=DTC  actual=24.0MHz` | `tx=DMA  actual=20.0MHz` | 后端与真实时钟都变了（80 MHz 分频档没有 24 MHz） |
| 标题文字 | `UNO R4 WiFi` | `Nucleo F410RB` | 只换板名，字号/位置/颜色不变 |
| 背光 | 硬件飞线强制拉高，`SCREEN_PIN_BL = -1`，固件不碰 D6 | 固件驱动 D6/PB10：初始化配成输出并拉低，setup 后拉高 | 这块板子的背光由固件驱动 |
| 背光断言 | “D6 始终未被固件操作” | “初始化拉低、setup 后拉高，全程只碰 3 次” | 同上 |
| 触摸 I2C | 无触摸，总线完全不配置 | 无触摸，但 I2C1 仍按通用流程初始化一次（占位） | 沿用样板 `screen.cpp` 的初始化顺序 |

面板专属数据（初始化表、宽高、`madctl[4]`、GRAM 偏移、像素格式、`spi_hz`、`spi_mode`）、
`gfx/`、`image.h`、`fonts.*`、`assets/logo.h`、`demo_app.cpp` 的画面与文字均逐字照搬
UNO R4 源工程，只有上表列出的板级/入口差异。

## 验证

### 已执行

1. 主机端行为验证（`bash ./test/run.sh`，g++ 编译 `src/screen` + `src/app` 并喂给虚拟 ST7789P3）：
   **全部通过（失败 0 项）**，覆盖
   - 初始化命令序列（25 条 = 24 条面板表 + 1 条方向 0 的 MADCTL）、各命令数据长度、`0x3A=0x05`、`0x11` 后 120 ms 延时、首条 `0xD0` 后 10 ms 延时；
   - 背光脚 D6：初始化只碰 2 次（pinMode + 拉低），setup 后拉高（第 3 次），整个演示期间无多余操作；
   - LCD SPI 请求时钟为 24 MHz（面板上限）、`touch=none`、`bpp=2`、`240x240`；
   - LOGO 展开 52800 像素（= 240x220）、画在 (0,10)、上下各留 10 行黑边；
   - 文字页 4 段文字（24 号白字/16 号蓝字 x2/32 号中文红字）的笔画像素与位置、背景为黑；
   - 5 个纯色、6 条彩条（条宽 40）、24 级灰阶逐格递增且首黑末白；
   - 一整圈（16 s）后回到 LOGO 画面（清屏 + LOGO = 110400 像素）；
   - `Screen_HasTouch() == false`，`Screen_TouchRead()` / `Screen_TouchReadRaw()` 返回 0。

   结果图 `test/out/*.png` 可人工核对：`1-logo.png`（LOGO）、`2-text.png`（文字）、
   `3-red.png`（纯色）、`4-colorbar.png`（彩条）、`5-grayscale.png`（灰阶）、
   `6-logo-loop.png`（第二圈 LOGO）。
2. Keil 实机工程编译（Arm Compiler 6.22）：
   `Program Size: Code=17864 RO-data=29912 RW-data=52 ZI-data=13692`，0 error / 0 warning，
   已生成 `*.axf` 与 `*.hex`。

### 需要实物确认（未做）

- SPI 实际波形与 20 MHz 下的时序裕量、背光电流与亮度；
- ST7789P3 实际显色与 Gamma（初始化表按参考工程逐条转写，未实测）；
- **3.3 V 主机侧电平裕量**：D13/D11/D10/D9/D8/D6 都要经过 5 V 参考的电平转换器，
  若出现花屏、不亮、随机色点，先量 D13 的高电平（应接近 3.3 V），再按“已知限制”处理；
- 主机端测试走的是逐字节回退路径（`tx=byte`），DMA 路径只在实物上运行，
  串口必须打印 `tx=DMA`；
- 本模组无触摸，因此没有触摸芯片 ID / 坐标需要实物核对。

## 已知限制

- **电平门限余量只有 50 mV**：转接板 U1/U2 的 B 侧接 +5 V，输入高门限 3.25 V（TXB0108
  SCES643L 第 5 节），而 Nucleo 的 3.3 V 输出只高出 50 mV。这是硬件决定的，固件无法改善；
  如果实测点不亮，可选的处理顺序是：
  1. 用示波器确认 D13/D11 高电平是否到 3.3 V（排线/焊点接触不良更常见）；
  2. 把 U1/U2 的 VCCB 从 +5 V 改接到 +3.3 V（需要割线飞线，改完 B 侧门限降到 2.15 V）；
  3. 换用原生 3.3 V 的转接板/排线。
- **SPI 实际时钟是 20 MHz，不是请求的 24 MHz**：APB2 = 80 MHz，分频档只有 40/20/10 MHz，
  `/4` 落在 20.0 MHz。若实物出现随机色点、错行或偶发花屏，把 `SCREEN_SPI_HZ` 改为
  `10000000UL`（回到 `/8`）再试。
- 背光只能开/关：PB10 在 F410 上没有可用的定时器通道，`SCREEN_BL_PWM` 为 0，
  `Screen_BacklightLevel()` 只按 0/255 处理。亮度由转接板 5 V + 面板背光电路决定，
  不代表规格书亮度。
- 整屏填充 240x240 需要发 115 200 字节，20 MHz 下纯线路传输下限约 46 ms，
  纯色/灰阶画面上能看到自上而下的推进。
- 字符宽度按 `sizey/2`，超出屏幕的字模整字丢弃（不会写出窗口外）。
- I2C1（PB8/PB9）本屏没有从机器件，但仍会被初始化一次；转接板 U2 的 10 k 上拉到 +5 V，
  这两个脚必须 5 V 容忍，详见“已知限制”中的电平说明。

## 资产来源

- `Drivers/`：STM32Cube_FW_F4_V1.28.3（BSD-3-Clause）；
- `src/screen/gfx/fonts.{h,cpp}`：参考工程 `HARDWARE/LCD/lcdfont.h`，只把中文索引从 GBK
  改成 UTF-8 三字节，点阵数据未动；
- `src/screen/assets/logo.h`：参考工程 `HARDWARE/LCD/pic.h` 的 `gImage_logo`（240x220 RGB565，
  前 8 字节图像头），用 `tools/pic2rle.py` 转成 RGB565 RLE：

```sh
python tools/pic2rle.py <参考工程>/HARDWARE/LCD/pic.h 240 220 src/screen/assets/logo.h --skip 8 --bpp 2
```

`pic.h` 里有 4 个图像数组，工具会按“数据长度 == 宽*高*bpp”自动选中 `gImage_logo`；
`--bpp 2` 表示每像素 2 字节（RGB565，默认 3 为 RGB888）。

- `src/screen`、`src/app`、`test/host`：移植自 `arduino-uno-r4-wifi/P154H007`
  （其自身又来自 `stm32g0b1rc-P154H007`）；
- `MDK-ARM/startup_stm32f410rx.s`、工程骨架：STM32Cube_FW_F4 的 Nucleo-F410RB 模板。
