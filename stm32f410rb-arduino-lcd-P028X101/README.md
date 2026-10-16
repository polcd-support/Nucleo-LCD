# stm32f410rb-arduino-lcd-P028X101

Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 + P028X101
（ST7789 240x320 + FT6236 电容触摸）的点屏与触摸演示。

工程由 `arduino-uno-r4-wifi/P028X101` 移植而来：效果、屏幕层分层、初始化寄存器表、
触摸换算、手写线条保护全部保持一致，只把“板卡 + 总线后端”换成 Nucleo-F410RB
（Arduino Uno V3 排针 + STM32 HAL）。

```
LOGO 3s -> 多字号文字 2s -> 全屏纯色 5 色 x 1s -> 彩条 3s -> 灰阶 3s -> 手写画线
```

与 STM32 参考工程 `stm32g0b1rc-*` 的 `LCD_DEMO()` 一致：本屏的 demo 上电先显示 RLE LOGO
（240x220），再进入文字页；LOGO/图片/倒计时三个状态在那份参考代码里同样不可达，
但本屏的 UNO R4 工程把 LOGO 做成了起始状态，这里照搬。

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
| 触摸 SDA | TP_SDA | SDA 位 | PB9 | I2C1_SDA，经 U2 TXS0104E 通道 A2/B2 |
| 触摸 SCL | TP_CLK | SCL 位 | PB8 | I2C1_SCL，U2 通道 A1/B1 |
| 触摸 INT | TP_INT | D4 | PB5 | 本驱动轮询，未使用 |
| 触摸 RESET | TP_RST | D5 | PB4 | U2 通道 A4/B4 |
| 调试串口 | — | D0/D1 | PA3/PA2 | USART2，接 ST-Link 虚拟串口，115200 8N1 |

本屏带触摸，四根触摸线（SDA/SCL/INT/RST）都要接；ST7789 只需要写方向，MISO 未使用。

转接板上 J4 是触摸侧的调试排针（1=RST 2=INT 3=SDA 4=SCL 5=GND 6=VDD），
示波器/逻辑分析仪可以直接从这里扎针。

转接板 U1/U2 的 OE 固定接 3.3 V，电平转换器始终使能，固件不控制它。
底板的 3V3/5V/GND 由 J2 从 Nucleo 的 Arduino 电源排针取电。

### 与 UNO R4 工程的板级差异

1. **电平参考点**：转接板的 B 侧（主机侧）按 5 V 主机设计（VCCB = +5 V），Nucleo 的
   I/O 是 3.3 V。TXB0108 数据手册（SCES643L 第 5 节）规定的输入高门限是
   `VIH(min) = VCCI x 0.65`，即 B 侧 3.25 V；3.3 V 输出只高出约 50 mV，属于“勉强合规”。
   点亮失败时先查这里，详见“已知限制”。
2. **背光**：UNO R4 板上 D6 走不通，实物用飞线把背光强制拉高；Nucleo 这边 D6（PB10）是
   空闲脚，固件直接驱动它（`SCREEN_PIN_BL = 6`）。如果转接板上还留着那根飞线，也不冲突。
3. **触摸总线**：转接板把触摸 I2C 引到 UNO R3 的 SDA/SCL 位上，而 Nucleo-64 的这两个位置
   正好是 I2C1 的默认引脚（PB9/PB8），所以这里用**硬件 I2C**，不是软件模拟。

## 总线与时钟

| 项 | 配置 |
|---|---|
| SYSCLK | 80 MHz（HSI 16 MHz -> PLL /16 x160 /2） |
| APB1 / APB2 | 40 MHz / 80 MHz |
| LCD SPI | SPI1，只发（1LINE），8 位帧、MSB first，**请求 24 MHz（面板上限）**，APB2/4 = **20.0 MHz** |
| 像素搬运 | DMA2_Stream3 / Channel3（SPI1_TX），每块完成后才返回 |
| 触摸 I2C | I2C1，100 kHz 标准模式，7 位地址 0x38 |
| 调试串口 | USART2 115200（ST-Link 虚拟串口） |
| Flash / RAM | 2 wait states；Code 23556 B + RO 29776 B，ZI 13696 B |

选 80 MHz 而不是 F410 的上限 100 MHz，是为了让 24 MHz 请求落到 APB2/4 = 20.0 MHz 这一档：
100 MHz 下只能取 25 MHz（超过面板上限）或 12.5 MHz（慢一倍）。
`Panel_P028X101.spi_hz` 与板级 `SCREEN_SPI_HZ` 都写 24 MHz，实际取两者较小者 = 24 MHz 请求，
再由 `bus/lcd_spi_stm32f4.cpp` 向下取不超过请求值的最快分频，得到 20.0 MHz。

## 编译与烧录

用 Keil uVision5（本机 MDK Plus 5.41 + Arm Compiler 6.22 + Keil.STM32F4xx_DFP 3.0.0）：

1. 打开 `MDK-ARM/stm32f410rb-arduino-lcd-P028X101.uvprojx`；
2. F7 编译（0 error / 0 warning）；
3. 调试器已配成 ST-Link（SWD，10 MHz），F8 下载并复位运行；
4. 串口助手打开 ST-Link 的虚拟串口，115200。

命令行等价写法：

```powershell
& "F:\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\stm32f410rb-arduino-lcd-P028X101.uvprojx" -o build.log
```

产物在 `MDK-ARM/stm32f410rb-arduino-lcd-P028X101/`：`*.axf`（调试）、`*.hex`（可烧）、`*.map`。
用 OpenOCD 烧 hex：

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program MDK-ARM/stm32f410rb-arduino-lcd-P028X101/stm32f410rb-arduino-lcd-P028X101.hex verify reset exit"
```

`Core/` 下的 HAL 取自 `STM32Cube_FW_F4_V1.28.3`（BSD-3-Clause，见
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`），只保留了本工程实际编译的模块
（见 `Core/Inc/stm32f4xx_hal_conf.h`）。本次没有用 CubeMX 生成工程，因此仓库里没有 `.ioc`。

上电自检输出（115200）：

```
screen: P028X101  panel=P028X101 (ST7789 240x320)  240x320  rotation=0  spi=24MHz  bpp=2  tx=DMA actual=20.0MHz  touch=FT6336U @0x38
  chip=0x64
```

`spi=24MHz` 是请求值（受 `Panel_P028X101.spi_hz` 和板级 `SCREEN_SPI_HZ` 限制），
`actual=20.0MHz` 是 APB2 分频后的真实时钟。`tx=DMA` 表示走 DMA2 搬运；若显示
`tx=byte(err=...)` 说明 DMA 后端没起来，屏幕仍会用逐字节回退路径工作，但慢很多。
`chip` 读不到或不是 0x64/0x11 一类 FT6xxx 值，说明触摸 I2C 没通（先查排线方向和 SDA/SCL）。

## 目录结构

```
Core/                              系统时钟、HAL 配置、MSP（所有引脚复用集中在这里）、中断
Drivers/                           STM32Cube HAL + CMSIS（来自 STM32Cube_FW_F4_V1.28.3）
src/arduino/                       Arduino API 兼容层（pinMode/digitalWrite/delay/millis/Serial/SPI/Wire）
src/app/demo_app.{h,cpp}           LCD_DEMO() 的状态机与各画面
src/screen/                        屏幕模块（换屏只动这里）
  screen_config.h                  选板卡、选屏幕、默认方向、缓冲区大小
  screen.{h,cpp}                   对外接口：初始化时序、SPI 像素流水线、旋转、触摸换算
  screen_device.h                  数据模型：面板 / 触摸驱动 / 设备
  screen_devices.h                 设备注册表（新增屏幕登记处）
  boards/nucleo_f410rb.h           板卡引脚与总线映射
  bus/lcd_spi_stm32f4.{h,cpp}      SPI1 + DMA2_Stream3 发送后端（含时钟分频计算）
  panels/panel_p028x101.{h,cpp}    ST7789 面板描述与初始化表
  touch/touch.h                    触摸驱动接口
  touch/touch_ft6336u.{h,cpp}      FT6336U（FT6xxx 寄存器，兼容 FT6236，本模组默认）
  touch/touch_cst816.{h,cpp}       CST816S/T（同款 FPC 排线的其它模组）
  gfx/gfx.{h,cpp}                  绘图 API（LCD_*），移植自参考工程 lcd.c
  gfx/fonts.{h,cpp}                ASCII 12/16/24/32 与中文点阵（来自参考工程 lcdfont.h）
  gfx/image.h                      RLE 图像结构 + LCD_ShowImage()
  assets/logo.h                    LOGO RLE 资产（240x220，RLE 约 14 KB）
test/host/                         主机端验证（Arduino 桩件 + 虚拟 ST7789）
test/run.sh                        编译并运行主机端验证，结果图输出到 test/out/
tools/pic2rle.py                   把 Image2Lcd 风格的 C 数组转成 RLE 资产
MDK-ARM/                           Keil 工程（startup、uvprojx/uvoptx、编译产物）
```

`src/` 与 `src/arduino/` 的分工：屏幕层只认“Arduino 风格”的 API，实物上由
`src/arduino/*` 落到 HAL，主机上由 `test/host/*` 落到虚拟面板，两边编译同一份 `src/screen`。

## 分层约定

- `app` 只调用 `gfx`（`LCD_*`）和 `screen`（`Screen_*`），不碰寄存器。
- `gfx` 颜色统一 RGB565；`screen` 负责换算成面板真正需要的格式（本面板即 RGB565，2 字节/像素）。
- 面板只描述“寄存器怎么写”，触摸只描述“寄存器怎么读”，方向修正（翻转/交换/偏移）写在设备里。
- 引脚复用集中在 `Core/Src/stm32f4xx_hal_msp.c`；LCD 的 CS/DC/RST/背光和触摸的 RST/INT 是普通
  GPIO，由 `pinMode/digitalWrite` 在运行时配置。
- 一次绘制的正确姿势：

```cpp
Screen_SetWindow(x, y, x + w - 1, y + h - 1);   // 设置窗口，CS 保持有效
Screen_WritePixels565(pixels, w * h);            // 由它释放 CS
```

## 与移植前工程的差异（除板卡外）

| 项 | arduino-uno-r4-wifi/P028X101 | 本工程 | 原因 |
|---|---|---|---|
| 总线后端 | `src/screen/bus/spi_dma_ra4m1.*`（SPI0 + DTC） | `src/screen/bus/lcd_spi_stm32f4.*`（SPI1 + DMA2） | 换芯片，接口契约（Begin/Write/LastError/ActualHz）不变 |
| 后端名 | `ScreenSpiDma_*` | `LcdSpi_*` | 名字不再绑定某个芯片的搬运器 |
| 后端编译开关 | `SCREEN_SPI_DMA && defined(ARDUINO_ARCH_RENESAS_UNO)` | `SCREEN_SPI_DMA && defined(STM32F410Rx)` | 主机端测试没有该宏，自动只编译逐字节回退路径 |
| 自检行 | `tx=DTC ... actual=24.0MHz` | `tx=DMA ... actual=20.0MHz` | 后端与真实时钟都变了 |
| 标题文字 | `UNO R4 WiFi` | `Nucleo F410RB` | 只换板名，字号/位置/颜色不变 |
| 背光断言 | “D6 始终未被固件操作” | “初始化拉低、setup 后拉高，全程只碰 3 次” | 这块板子的背光由固件驱动 |

其余面板表、触摸驱动、`gfx/`、`assets/`、`demo_app.cpp` 的画面与文字、`pic2rle.py`
与 UNO R4 源工程逐字一致（`test/host/*` 只改了背光相关的桩件与断言）。

## 触摸标定

与参考工程一致：240x320、不交换、不翻转、无偏移（方向 0 下原生坐标即显示坐标）。
方向不对时改 `panels/panel_p028x101.cpp` 里 `p028x101_touch` 的
`swap_xy`、`invert_x`、`invert_y`、`offset_x`、`offset_y`。
`Screen_TouchReadRaw()` 返回未换算的原生坐标，便于对照标定。
方向 2/3（横屏）的触摸换算是按 MADCTL 的 `MV` 位推出来的，实物核对前请先只在方向 0 下用。

## 面板说明

- 初始化表照搬参考工程 `HARDWARE/LCD/lcd_init.c`；`0x3A = 0x05`（16-bit/pixel，RGB565）。
- 参考工程把 `0x36 (MADCTL)` 硬编码为 `0x00`，另外三个方向按 ST7789 标准补全为
  `0x00 / 0xC0 / 0x70 / 0xA0`。
- GRAM 就是 240x320，四个方向都不需要行列偏移（`offset_x/offset_y` 全 0）。
- 参考工程初始化末尾用 `read_lcd_id()` 决定写 `0x20` 还是 `0x21`；转接板没接 MISO，
  读不到 ID，这里固定写 `0x21`（Display Inversion On）。若实物反相，改成 `0x20`。
- 面板侧 `spi_hz = 24000000UL`，SPI 为 mode 0（参考工程 CPOL=0/CPHA=0）。

## 验证

### 已执行

1. 主机端行为验证（`bash ./test/run.sh`，g++ 编译 `src/screen` + `src/app` 并喂给虚拟 ST7789）：
   **全部通过（失败 0 项）**，覆盖
   - 初始化命令序列（16 条：面板表 15 条 + `apply_rotation(0)` 补写的 1 条 `0x36`）、
     各命令数据长度、首条 `0x36 = 0x00`、`0x3A = 0x05`、`0x11` 后 120 ms 延时、`0x21` 在 `0x29` 前；
   - 背光脚 D6：初始化只碰 2 次（pinMode + 拉低），setup 后拉高，整个演示期间共 3 次；
   - LCD SPI 请求时钟为 24 MHz（面板上限），FT6336U 配置为阈值 40、轮询模式；
   - LOGO 展开 52800 像素（240x220）且四角为白，随后文字页覆盖；
   - 5 个纯色、6 条彩条（条宽 40）、24 级灰阶的每个像素值；
   - 手写笔画、Clear 按钮清屏；
   - 触摸坐标在方向 0/1/2/3 下的换算（240x320 / 320x240）与抬手判定。
   共 49 项断言，全部为 `[ok  ]`。

   结果图 `test/out/*.png` 已人工核对：`1-logo.png`（LOGO）、`2-text.png`（文字）、
   `3-red.png`（纯色）、`4-colorbar.png`（彩条）、`5-grayscale.png`（灰阶）、
   `6-handwriting.png`（手写）、`7-cleared.png`（清屏）、`8-rotation2.png`（横屏）。
2. Keil 实机工程编译（Arm Compiler 6.22）：
   `Program Size: Code=23556 RO-data=29776 RW-data=56 ZI-data=13696`，0 error / 0 warning，
   已生成 `*.axf` 与 `*.hex`。

### 需要实物确认（未做）

- SPI/I2C 实际波形与时钟裕量、背光电流与亮度；
- FT6236/FT6336U 的 chip ID 与真实触摸坐标：只有串口读到非零芯片 ID 并返回坐标才算触摸通过，
  软件不会用硬编码 ID 或假坐标绕过；
- ST7789 实际显色、Gamma 与反相极性（`0x21` vs `0x20`）；
- **3.3 V 主机侧电平裕量**：D13/D11/D10/D9/D8/D6 以及触摸 SDA/SCL 都要经过 5 V 参考的
  电平转换器，若出现花屏、不亮、随机色点，先量 D13 的高电平（应接近 3.3 V），
  再按“已知限制”处理；
- 主机端测试走的是逐字节回退路径（`tx=byte`），DMA 路径只在实物上运行，
  串口必须打印 `tx=DMA`。

## 已知限制

- **电平门限余量只有 50 mV**：转接板 U1/U2 的 B 侧接 +5 V，输入高门限 3.25 V（TXB0108
  SCES643L 第 5 节），而 Nucleo 的 3.3 V 输出只高出 50 mV。这是硬件决定的，固件无法改善；
  如果实测点不亮，可选的处理顺序是：
  1. 用示波器确认 D13/D11 高电平是否到 3.3 V（排线/焊点接触不良更常见）；
  2. 把 U1/U2 的 VCCB 从 +5 V 改接到 +3.3 V（需要割线飞线，改完 B 侧门限降到 2.15 V）；
  3. 换用原生 3.3 V 的转接板/排线。
- **触摸总线 B 侧被 10 k 上拉到 +5 V**：I2C1 用的 PB8/PB9 必须能承受 5 V。F410 数据手册
  里 I2C 引脚一般标为 5 V 容忍（FT），但本仓库没有随附数据手册、也未实测，
  请实物核对一次；不确定时把 U2 的 VCCB 改接 +3.3 V（A 侧本来就是 3.3 V）。
- 背光只能开/关：PB10 在 F410 上没有可用的定时器通道，`SCREEN_BL_PWM` 为 0，
  `Screen_BacklightLevel()` 只按 0/255 处理。亮度由转接板 5 V + 面板背光电路决定，
  不代表规格书亮度。
- 请求 24 MHz，实际 20.0 MHz（APB2/4）：整屏填充 240x320 需要发 153 600 字节，
  20 MHz 下纯线路传输下限约 0.06 s，比 10 MHz 板少一半，但纯色/灰阶画面上仍能看到
  自上而下的推进。
- 字符宽度按 `sizey/2`，超出屏幕的字模整字丢弃（不会写出窗口外）。

## 资产来源

- `Drivers/`：STM32Cube_FW_F4_V1.28.3（BSD-3-Clause）；
- `src/screen/gfx/fonts.{h,cpp}`：参考工程 `HARDWARE/LCD/lcdfont.h`，只把中文索引从 GBK
  改成 UTF-8 三字节，点阵数据未动；
- `src/screen/assets/logo.h`：参考工程 `HARDWARE/LCD/pic.h` 的 `gImage_logo[]`
  （RGB565，跳过 8 字节图像头），用
  `python tools/pic2rle.py <pic.h> 240 220 src/screen/assets/logo.h --skip 8 --bpp 2` 生成；
- `src/screen`、`src/app`、`test/host`：移植自 `arduino-uno-r4-wifi/P028X101`
  （其自身又来自 `stm32g0b1rc-*` 系列参考工程）；
- `MDK-ARM/startup_stm32f410rx.s`、工程骨架：STM32Cube_FW_F4 的 Nucleo-F410RB 模板。
