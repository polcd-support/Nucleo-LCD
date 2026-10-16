# stm32f410rb-arduino-lcd-P020B025P-IPS-V2

Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 + P020B025P-IPS-V2
（ST7789P3 240x320 IPS，无触摸）的点屏演示。

工程由 `arduino-uno-r4-wifi/P020B025P-IPS-V2` 移植而来：显示效果、屏幕层分层、面板初始化
寄存器表全部保持一致，只把“板卡 + 总线后端”换成 Nucleo-F410RB（Arduino Uno V3 排针 + STM32 HAL）。

```
彩条 3s -> 灰阶 3s -> 循环
```

与参考工程 `stm32g0b1rc-P020B025P-IPS-V2` 的 `LCD_Display_RunTest()` 一致：这块屏本来就是
纯显示模组（无触摸、无文字、无图片），参考代码里也只有彩条与灰阶两个画面，因此本工程没有
文字页、LOGO、手写和 Clear 按钮。背光在 UNO R4 上是硬件常开，到这里改由固件驱动 D6/PB10
（初始化拉低，`DemoApp_Setup()` 里拉高）。

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
| 背光控制 | LCD_PWR | D6 | PB10 | 固件推挽输出：初始化拉低，画面准备好后拉高（U1 通道 A6/B6） |
| — | — | D7 | PA8 | 转接板未连接 |
| 触摸 SDA | TP_SDA | SDA 位 | PB9 | 本模组无触摸，无器件；固件只按样板初始化 I2C1 |
| 触摸 SCL | TP_CLK | SCL 位 | PB8 | 同上（U2 TXS0104E 通道 A1/B1） |
| 触摸 INT | TP_INT | D4 | PB5 | 本模组无触摸，`SCREEN_PIN_TOUCH_INT = -1`，固件不配置 |
| 触摸 RESET | TP_RST | D5 | PB4 | 本模组无触摸，`SCREEN_PIN_TOUCH_RST = -1`，固件不配置 |
| 调试串口 | — | D0/D1 | PA3/PA2 | USART2，接 ST-Link 虚拟串口，115200 8N1 |

`src/screen/boards/nucleo_f410rb.h` 里 `SCREEN_PIN_TOUCH_RST` / `SCREEN_PIN_TOUCH_INT` 都是 `-1`，
`screen.cpp` 里对应两段 `#if ... >= 0` 的配置代码不参与编译；设备表里 `ScreenDevice::touch`
是 `NULL`，`Screen_HasTouch()` 返回 `false`。注意 `Screen_Init()` 仍按样板调用
`Wire.begin()` / `setClock(100 kHz)`，所以 PB8/PB9 会被配成 I2C1 的复用功能（总线上没有器件，
固件不做任何访问）。

转接板 U1/U2 的 OE 固定接 3.3 V，电平转换器始终使能，固件不控制它。
底板的 3V3/5V/GND 由 J2 从 Nucleo 的 Arduino 电源排针取电。

### 与 UNO R4 工程的板级差异

1. **电平参考点**：转接板的 B 侧（主机侧）按 5 V 主机设计（VCCB = +5 V），Nucleo 的
   I/O 是 3.3 V。TXB0108 数据手册（SCES643L 第 5 节）规定的输入高门限是
   `VIH(min) = VCCI x 0.65`，即 B 侧 3.25 V；3.3 V 输出只高出约 50 mV，属于“勉强合规”。
   点亮失败时先查这里，详见“已知限制”。
2. **背光**：UNO R4 板上 D6 走不通，实物用飞线把背光强制拉高，固件全程不碰它；Nucleo 这边
   D6（PB10）是空闲脚，固件直接驱动它（`SCREEN_PIN_BL = 6`）。UNO R4 工程里“背光由外部硬件
   保持 255、固件从不写 D6”的两条断言，在这里换成“初始化只碰 2 次（pinMode + 拉低）、
   setup 后拉高、全程 3 次”。
3. **触摸总线**：转接板把触摸 I2C 引到 UNO R3 的 SDA/SCL 位上，而 Nucleo-64 的这两个位置
   正好是 I2C1 的默认引脚（PB9/PB8），所以这里用**硬件 I2C**，不是软件模拟。本模组无触摸，
   这条总线只被初始化，没有从机。

## 总线与时钟

| 项 | 配置 |
|---|---|
| SYSCLK | 80 MHz（HSI 16 MHz -> PLL /16 x160 /2） |
| APB1 / APB2 | 40 MHz / 80 MHz |
| LCD SPI | SPI1，只发（1LINE），8 位帧、MSB first，模式 0（CPOL=0/CPHA=0） |
| LCD SPI 时钟 | 请求 24 MHz（面板上限），APB2/4 = **20.0 MHz** 实际生效 |
| 像素搬运 | DMA2_Stream3 / Channel3（SPI1_TX），每块完成后才返回 |
| 触摸 I2C | I2C1，100 kHz 标准模式（本工程无触摸器件） |
| 调试串口 | USART2 115200（ST-Link 虚拟串口） |
| Flash / RAM | 2 wait states；Code 14236 B + RO 1184 B，RW 52 B，ZI 7804 B |

F410 的 SPI1 挂在 APB2（80 MHz）上，分频档只能是 /2 = 40 MHz、/4 = 20 MHz、/8 = 10 MHz……
面板（ST7789 系列）的写时钟上限是 24 MHz，`spi_hz` 也这么填，所以“不超过 24 MHz 的最快分档”
是 **/4 = 20.0 MHz**（`bus/lcd_spi_stm32f4.cpp` 的 `select_prescaler()` 就是这么选的）。
/2 = 40 MHz 超面板上限，不能用；想要 24 MHz 整，得另配 PLL/APB2，本工程沿用样板的 80 MHz。

## 编译与烧录

用 Keil uVision5（本机 MDK Plus 5.41 + Arm Compiler 6.22 + Keil.STM32F4xx_DFP 3.0.0）：

1. 打开 `MDK-ARM/stm32f410rb-arduino-lcd-P020B025P-IPS-V2.uvprojx`；
2. F7 编译（0 error / 0 warning）；
3. 调试器已配成 ST-Link（SWD，10 MHz），F8 下载并复位运行；
4. 串口助手打开 ST-Link 的虚拟串口，115200。

命令行等价写法：

```powershell
& "F:\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\stm32f410rb-arduino-lcd-P020B025P-IPS-V2.uvprojx" -o build.log
```

产物在 `MDK-ARM/stm32f410rb-arduino-lcd-P020B025P-IPS-V2/`：`*.axf`（调试）、`*.hex`（可烧）、`*.map`。
用 OpenOCD 烧 hex：

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program MDK-ARM/stm32f410rb-arduino-lcd-P020B025P-IPS-V2/stm32f410rb-arduino-lcd-P020B025P-IPS-V2.hex verify reset exit"
```

`Core/` 下的 HAL 取自 `STM32Cube_FW_F4_V1.28.3`（BSD-3-Clause，见
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`），只保留了本工程实际编译的模块
（见 `Core/Inc/stm32f4xx_hal_conf.h`）。本次没有用 CubeMX 生成工程，因此仓库里没有 `.ioc`。

上电自检输出（115200）：

```
screen: P020B025P-IPS-V2  panel=P020B025P-IPS-V2 (ST7789P3 240x320)  240x320  rotation=0  spi=24MHz  bpp=2  tx=DMA actual=20.0MHz  touch=none
```

`spi=24MHz` 是请求值（受 `Panel_P020B025P_IPS_V2.spi_hz` 和板级 `SCREEN_SPI_HZ` 限制），
`actual=20.0MHz` 是 APB2 分频后的真实时钟。`tx=DMA` 表示走 DMA2 搬运；若显示
`tx=byte(err=...)` 说明 DMA 后端没起来，屏幕仍会用逐字节回退路径工作，但慢很多。
`touch=none` 表示设备表里没有触摸驱动——本模组本来就没有触摸 IC，不是初始化失败。

## 目录结构

```
Core/                              系统时钟、HAL 配置、MSP（所有引脚复用集中在这里）、中断
Drivers/                           STM32Cube HAL + CMSIS（来自 STM32Cube_FW_F4_V1.28.3）
src/arduino/                       Arduino API 兼容层（pinMode/digitalWrite/delay/millis/Serial/SPI/Wire）
src/app/demo_app.{h,cpp}           LCD_Display_RunTest() 的状态机（彩条 <-> 灰阶）
src/screen/                        屏幕模块（换屏只动这里）
  screen_config.h                  选板卡、选屏幕、默认方向、缓冲区大小
  screen.{h,cpp}                   对外接口：初始化时序、SPI 像素流水线、旋转
  screen_device.h                  数据模型：面板 / 触摸驱动 / 设备
  screen_devices.h                 设备注册表（新增屏幕登记处）
  boards/nucleo_f410rb.h           板卡引脚与总线映射
  bus/lcd_spi_stm32f4.{h,cpp}      SPI1 + DMA2_Stream3 发送后端（含时钟分频计算）
  panels/panel_p020b025p_ips_v2.{h,cpp}  ST7789P3 面板描述与初始化表
  gfx/gfx.{h,cpp}                  绘图 API（LCD_*），移植自参考工程 lcd.c
  gfx/fonts.{h,cpp}                ASCII 12/16/24/32 与中文点阵（来自参考工程 lcdfont.h）
  gfx/image.h                      RLE 图像结构（本工程 demo 未用到图片，保留以备用）
test/host/                         主机端验证（Arduino 桩件 + 虚拟 ST7789P3）
test/run.sh                        编译并运行主机端验证，结果图输出到 test/out/
tools/pic2rle.py                   把 Image2Lcd 风格的 C 数组转成 RLE 资产（--bpp 2/3）
MDK-ARM/                           Keil 工程（startup、uvprojx/uvoptx、编译产物）
```

本屏没有触摸驱动文件，因此没有 `src/screen/touch/` 目录；`screen.cpp` 里触摸相关的入口
都在判空后直接返回。

`src/` 与 `src/arduino/` 的分工：屏幕层只认“Arduino 风格”的 API，实物上由
`src/arduino/*` 落到 HAL，主机上由 `test/host/*` 落到虚拟面板，两边编译同一份 `src/screen`。

## 分层约定

- `app` 只调用 `gfx`（`LCD_*`）和 `screen`（`Screen_*`），不碰寄存器。
- `gfx` 颜色统一 RGB565；`screen` 负责换算成面板真正需要的格式（本面板本来就是 RGB565，直接发）。
- 面板只描述“寄存器怎么写”，方向修正（翻转/交换/偏移）写在面板描述里（本面板四个方向无偏移）。
- 引脚复用集中在 `Core/Src/stm32f4xx_hal_msp.c`；LCD 的 CS/DC/RST/背光是普通 GPIO，
  由 `pinMode/digitalWrite` 在运行时配置。
- 一次绘制的正确姿势：

```cpp
Screen_SetWindow(x, y, x + w - 1, y + h - 1);   // 设置窗口，CS 保持有效
Screen_WritePixels565(pixels, w * h);            // 由它释放 CS
```

## 触摸

本模组无触摸：`ScreenDevice::touch == NULL`，`Screen_HasTouch()` 返回 `false`，
`Screen_TouchRead()` 恒返回 0 个触点。触摸相关代码在 `screen.cpp` 中保留（判空后直接返回），
以后换带触摸的模组时补一个触摸驱动、把 `SCREEN_PIN_TOUCH_*` 改回 5/4，并重新做实物标定。

## 与移植前工程的差异（除板卡外）

| 项 | arduino-uno-r4-wifi/P020B025P-IPS-V2 | 本工程 | 原因 |
|---|---|---|---|
| 总线后端 | `src/screen/bus/spi_dma_ra4m1.*`（SPI0 + DTC） | `src/screen/bus/lcd_spi_stm32f4.*`（SPI1 + DMA2） | 换芯片，接口契约（Begin/Write/LastError/ActualHz）不变 |
| 后端名 | `ScreenSpiDma_*` | `LcdSpi_*` | 名字不再绑定某个芯片的搬运器 |
| 后端编译开关 | `SCREEN_SPI_DMA && defined(ARDUINO_ARCH_RENESAS_UNO)` | `SCREEN_SPI_DMA && defined(STM32F410Rx)` | 主机端测试没有该宏，自动只编译逐字节回退路径 |
| SPI 时钟 | 请求 24 MHz，PCLKA 48 MHz 精确得到 24.0 MHz | 请求 24 MHz，APB2 80 MHz 下实际 20.0 MHz | F410 的分频档只能取 40/20/10 MHz |
| 自检行 | `tx=DTC ... actual=24.0MHz` | `tx=DMA ... actual=20.0MHz` | 后端与真实时钟都变了 |
| 背光 | 硬件常开，固件全程不碰 D6 | 固件驱动 D6/PB10：初始化拉低、setup 后拉高，全程 3 次 | 这块板子的背光必须由固件开 |
| 背光断言 | “D6 始终未被固件操作” | “初始化只碰 2 次、setup 后 3 次，演示期间不再碰” | 同上 |
| `screen.cpp` | RA4M1 版：后端是 SPI0 + DTC | 样板版：后端是 SPI1 + DMA2 | P020B 的 UNO R4 `screen.cpp` 除总线后端外与样板一致，没有 RA4M1 专有改动需要取舍 |

标题文字、字号、颜色、彩条/灰阶的像素值全部未动（本 demo 本来就没有文字）。

## 验证

### 已执行

1. 主机端行为验证（`bash ./test/run.sh`，g++ 编译 `src/screen` + `src/app` 并喂给虚拟 ST7789P3）：
   **全部通过（31 项，失败 0 项）**，覆盖
   - 初始化 20 条命令（面板表 19 条 + `Screen_Init` 末尾应用方向的一条 `0x36`）的序列与
     各命令数据长度、`0x3A=0x05`、首条 `0x36=0x00`、`0x11` 后 120 ms、末尾 `0x2C` 后 20 ms；
   - 背光脚 D6：初始化只碰 2 次（pinMode + 拉低），setup 后拉高（第 3 次），
     整个演示期间无多余操作；初始化期间未写像素；
   - setup 清屏写满 240x320 像素；`Screen_HasTouch() == false`；
   - 彩条 6 条颜色与分界（x=20/60/100/140/180/220 取中点核对）、占满整屏高度，
     3 s 后切灰阶、6 s 后切回彩条；
   - 灰阶 24 级首格黑、末格白、逐格单调递增、占满整屏高度。
2. Keil 实机工程编译（Arm Compiler 6.22）：
   `Program Size: Code=14236 RO-data=1184 RW-data=52 ZI-data=7804`，0 error / 0 warning，
   已生成 `*.axf` 与 `*.hex`。

结果图 `test/out/*.png` 已人工核对：`1-colorbar.png`（彩条）、`2-grayscale.png`（灰阶）、
`3-colorbar-2.png`（第二轮回彩条）。

### 需要实物确认（未做）

- SPI 实际时钟与波形、20 MHz 下 ST7789P3 的写时序裕量；
- 面板实际显色与 Gamma、初始化表在这块屏上的最终观感（本工程只保证寄存器流与源工程一致）；
- 背光电流与亮度（D6/PB10 只做开/关）；
- **3.3 V 主机侧电平裕量**：D13/D11/D10/D9/D8/D6 都要经过 5 V 参考的电平转换器，若出现花屏、
  不亮、随机色点，先量 D13 的高电平（应接近 3.3 V），再按“已知限制”处理；
- **触摸未实物验证**：本模组无触摸 IC，本工程也没有可验证的触摸路径。换带触摸的模组时，
  必须先确认 PB8/PB9 的 5 V 容忍与 U2 电平转换（见下），再做坐标标定；
- 主机端测试走的是逐字节回退路径（`tx=byte`），DMA 路径只在实物上运行，
  串口必须打印 `tx=DMA`；`Screen_TouchRead()` 在无触摸时恒为 0 的能力由主机端断言覆盖。

## 已知限制

- **电平门限余量只有 50 mV**：转接板 U1/U2 的 B 侧接 +5 V，输入高门限 3.25 V（TXB0108
  SCES643L 第 5 节），而 Nucleo 的 3.3 V 输出只高出 50 mV。这是硬件决定的，固件无法改善；
  如果实测点不亮，可选的处理顺序是：
  1. 用示波器确认 D13/D11 高电平是否到 3.3 V（排线/焊点接触不良更常见）；
  2. 把 U1/U2 的 VCCB 从 +5 V 改接到 +3.3 V（需要割线飞线，改完 B 侧门限降到 2.15 V）；
  3. 换用原生 3.3 V 的转接板/排线。
- **本模组无触摸，但 I2C1 仍被初始化**：`Screen_Init()` 按样板调用 `Wire.begin()`，
  PB8/PB9 会被配成 I2C1 复用功能，而这两个脚在转接板上挂在 U2（触摸侧）的 B 侧，
  被 10 k 上拉到 +5 V。F410 数据手册里 I2C 引脚一般标为 5 V 容忍（FT），但本仓库没有
  随附数据手册、也未实测，实物上仍建议核对一次；不确定时把 U2 的 VCCB 改接 +3.3 V
  （A 侧本来就是 3.3 V）。
- 背光只能开/关：PB10 在 F410 上没有可用的定时器通道，`SCREEN_BL_PWM` 为 0，
  `Screen_BacklightLevel()` 只按 0/255 处理。亮度由转接板 5 V + 面板背光电路决定，
  不代表规格书亮度。
- 整屏填充 240x320 需要发 153 600 字节，20 MHz 下纯线路传输下限约 0.06 s，
  纯色/灰阶画面上能看到自上而下的推进。
- 面板侧写时钟上限 24 MHz，本板实际 20 MHz：余量只有 4 MHz，长排线/接触不良时先降
  `SCREEN_SPI_HZ` 到 `10000000UL`（APB2/8）看是否稳定。
- 字符宽度按 `sizey/2`，超出屏幕的字模整字丢弃（本 demo 未用到文字，能力保留）。

## 资产来源

- `Drivers/`：STM32Cube_FW_F4_V1.28.3（BSD-3-Clause）；
- `src/screen/gfx/fonts.{h,cpp}`：参考工程 `HARDWARE/LCD/lcdfont.h`，只把中文索引从 GBK
  改成 UTF-8 三字节，点阵数据未动；
- `src/screen/panels/panel_p020b025p_ips_v2.{h,cpp}`：初始化序列取自参考工程
  `HARDWARE/LCD/lcd_init.c`（BOE 2.0 寸 ST7789P3），与 `arduino-uno-r4-wifi/P020B025P-IPS-V2`
  中的同名文件逐字一致；
- `src/app`、`src/screen/gfx`、`test/host`、`tools/pic2rle.py`：移植自
  `arduino-uno-r4-wifi/P020B025P-IPS-V2`；
- `MDK-ARM/startup_stm32f410rx.s`、工程骨架：STM32Cube_FW_F4 的 Nucleo-F410RB 模板。

本工程 demo **不含图片资产**，因此没有 `src/screen/assets/`；`gfx/image.h`、`LCD_ShowImage()`
与 `tools/pic2rle.py` 作为绘图能力保留。若以后要加图，参考工程 `HARDWARE/LCD/pic.h` 的
`gImage_img1/gImage_logo` 图像本体是 RGB565（数组长度 = 8 + W*H*2），命令形如：

```sh
python tools/pic2rle.py <参考工程>/HARDWARE/LCD/pic.h <宽> <高> src/screen/assets/xxx.h --skip 8 --bpp 2
```
