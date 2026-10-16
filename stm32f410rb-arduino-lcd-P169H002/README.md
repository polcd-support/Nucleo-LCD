# stm32f410rb-arduino-lcd-P169H002

Nucleo-F410RB + “Arduino-UNO-R4-WIFI-SPI 通用转接板” Rev 1.3 + P169H002
（ST7789 240x280 IPS + CST816 电容触摸）的点屏与触摸演示。

工程由 `arduino-uno-r4-wifi/P169H002` 移植而来：效果、屏幕层分层、初始化寄存器表、
触摸换算、手写线条保护全部保持一致，只把“板卡 + 总线后端”换成 Nucleo-F410RB
（Arduino Uno V3 排针 + STM32 HAL）。

```
LOGO 3s -> 多字号文字 2s -> 全屏纯色 5 色 x 1s -> 彩条 3s -> 灰阶 3s -> 手写画线
```

与 STM32 参考工程 `stm32g0b1rc-P169H002` 的 `LCD_DEMO()` 一致：上电先放 240x220 的 LOGO，
再进入文字页；`STATE_IMAGE` / `STATE_COUNTDOWN` 在那份参考代码里不可达，因此没有移植。

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
| 触摸 INT | TP_INT | D4 | PB5 | 本驱动轮询，未使用（仍配成输入） |
| 触摸 RESET | TP_RST | D5 | PB4 | U2 通道 A4/B4，CST816 复位用 |
| 调试串口 | — | D0/D1 | PA3/PA2 | USART2，接 ST-Link 虚拟串口，115200 8N1 |

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
   空闲脚，固件直接驱动它（`SCREEN_PIN_BL = 6`）。如果转接板上还留着那根飞线，也不冲突
   （固件拉高只是与飞线同向；要调光时得先拆飞线，见“已知限制”）。
3. **触摸总线**：转接板把触摸 I2C 引到 UNO R3 的 SDA/SCL 位上，而 Nucleo-64 的这两个位置
   正好是 I2C1 的默认引脚（PB9/PB8），所以这里用**硬件 I2C**，不是软件模拟。

## 总线与时钟

| 项 | 配置 |
|---|---|
| SYSCLK | 80 MHz（HSI 16 MHz -> PLL /16 x160 /2） |
| APB1 / APB2 | 40 MHz / 80 MHz |
| LCD SPI | SPI1，只发（1LINE），8 位帧、MSB first，mode 0，APB2/4 = **20.0 MHz**（请求 24 MHz） |
| 像素搬运 | DMA2_Stream3 / Channel3（SPI1_TX），每块完成后才返回 |
| 触摸 I2C | I2C1，100 kHz 标准模式，7 位地址 0x15 |
| 调试串口 | USART2 115200（ST-Link 虚拟串口） |
| Flash / RAM | 2 wait states；Code 24216 B + RO-data 29840 B，RW-data 56 B，ZI-data 13712 B |

请求 24 MHz 是这块屏在 UNO R4 上实测的高速档（`Panel_P169H002.spi_hz`），
F410 的 SPI1 只能取 `PCLK2 / 2^n`：`/2 = 40 MHz` 超出请求值，于是取最近的合法档
`/4 = 20.0 MHz`。选 80 MHz SYSCLK 而不是 F410 的上限 100 MHz，是因为 100 MHz 下
`/4 = 25 MHz` 同样超出 24 MHz（只能退到 12.5 MHz），80 MHz 下的 20 MHz 最接近请求值。

`Core/Src/main.c` 的注释块沿用板级模板的口径（那里按模板面板的 10 MHz 档写的）；
本工程的实际分频以上表为准，板级基础设施与样板逐字一致，未做屏专属改动。

## 编译与烧录

用 Keil uVision5（本机 MDK Plus 5.41 + Arm Compiler 6.22 + Keil.STM32F4xx_DFP 3.0.0）：

1. 打开 `MDK-ARM/stm32f410rb-arduino-lcd-P169H002.uvprojx`；
2. F7 编译（0 error / 0 warning）；
3. 调试器已配成 ST-Link（SWD，10 MHz），F8 下载并复位运行；
4. 串口助手打开 ST-Link 的虚拟串口，115200。

命令行等价写法：

```powershell
& "F:\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\stm32f410rb-arduino-lcd-P169H002.uvprojx" -o build.log
```

产物在 `MDK-ARM/stm32f410rb-arduino-lcd-P169H002/`：`*.axf`（调试）、`*.hex`（可烧）、`*.map`。
用 OpenOCD 烧 hex：

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program MDK-ARM/stm32f410rb-arduino-lcd-P169H002/stm32f410rb-arduino-lcd-P169H002.hex verify reset exit"
```

`Core/` 下的 HAL 取自 `STM32Cube_FW_F4_V1.28.3`（BSD-3-Clause，见
`Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`），只保留了本工程实际编译的模块
（见 `Core/Inc/stm32f4xx_hal_conf.h`）。本次没有用 CubeMX 生成工程，因此仓库里没有 `.ioc`。

上电自检输出（115200）：

```
touch init: chip=0xB5
  disable_auto_sleep=ok
screen: P169H002  panel=P169H002 (ST7789 240x280)  240x280  rotation=0  spi=24MHz  bpp=2  tx=DMA actual=20.0MHz  touch=CST816 @0x15
```

`spi=24MHz` 是请求值（受 `Panel_P169H002.spi_hz` 和板级 `SCREEN_SPI_HZ` 限制），
`actual=20.0MHz` 是 APB2 分频后的真实时钟。`tx=DMA` 表示走 DMA2 搬运；若显示
`tx=byte(err=...)` 说明 DMA 后端没起来，屏幕仍会用逐字节回退路径工作，但慢很多。
`chip` 读不到或不是 0xB5（CST816S）/0xB6（CST816T）一类值，说明触摸 I2C 没通
（先查排线方向和 SDA/SCL）。

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
  panels/panel_p169h002.{h,cpp}    ST7789 面板描述与初始化表
  touch/touch.h                    触摸驱动接口
  touch/touch_cst816.{h,cpp}       CST816S/T（0x15，本模组默认）
  touch/touch_ft6336u.{h,cpp}      FT6336U/FT6236（同款 FPC 排线的其它模组可选）
  gfx/gfx.{h,cpp}                  绘图 API（LCD_*），移植自参考工程 lcd.c
  gfx/fonts.{h,cpp}                ASCII 12/16/24/32 与中文点阵（来自参考工程 lcdfont.h）
  gfx/image.h                      RLE 图像结构
  assets/logo.h                    LOGO 资产（240x220，RLE）；纯头文件，由 demo_app.cpp 包含，不进 Keil 文件列表
test/host/                         主机端验证（Arduino 桩件 + 虚拟 ST7789 240x280）
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

## 新增一块屏

1. 在 `src/screen/panels/` 加 `panel_xxx.{h,cpp}`，填写 `ScreenPanel`：分辨率、初始化序列、
   4 个方向的 `madctl`/GRAM 偏移、像素格式（RGB565 或 RGB666）、建议 SPI 时钟与模式。
   `.cpp` 用 `#if SCREEN_DEVICE == SCREEN_DEVICE_XXX` 包起来，未选中的屏不参与编译。
2. 触摸若不是 CST816/FT6336U，在 `src/screen/touch/` 加一个实现 `ScreenTouchDriver` 的文件，
   同样用 `#if SCREEN_TOUCH_DRIVER == ...` 包起来。
3. 在 `src/screen/screen_devices.h` 的表里加一行。
4. 在 `src/screen/screen_config.h` 里把 `SCREEN_DEVICE`（和需要时的 `SCREEN_TOUCH_DRIVER`）指过去。
5. 板卡引脚或时钟档不同就改 `src/screen/boards/nucleo_f410rb.h`。

面板把颜色按 RGB565 发还是按 RGB666 发，由 `ScreenPanel::pixel_format` 决定，上层代码不用改；
写像素、填充、文字、图像都走同一条 `Screen_WritePixels565()` 流水线。

## 触摸标定

CST816 @ `0x15` 只做 I2C 轮询，不接也不使用转接板 D4/INT：

- 复位：`TP_RST`(D5) 拉低 10 ms -> 拉高 -> 等 200 ms（本块实物在 100 ms 时首次读可能不稳）；
- 初始化只写 `0xFE = 0x01`（禁止自动休眠）；不写自动休眠会在约 5 s 后 NACK，
  直到手指重新唤醒控制器（演示约 16 s 后才进手写页，所以必须禁止）；
- 采样：先单读 `0x02`（触点数，`0x00` 与 `0xFF`=睡眠态都算无触摸），再从 `0x03` 连读 4 字节
  得到 X/Y（各 12 位）。拆两次事务与 STM32 参考驱动一致，本模组不接受 0x01 起始的合并读。

默认与参考工程一致：240x280、不交换、不翻转、`offset_y = 5`（参考工程 `CST816.c` 的
`TOUCH_OFFSET_Y`；该文件里的 `#if REVERSE` 分支被 `REVERSE 0` 关掉，是死代码，本工程同样不翻转）。
方向 0 下原生坐标即显示坐标（仅 Y 加 5）。
方向不对时改 `panels/panel_p169h002.cpp` 里 `p169h002_touch` 的
`swap_xy`、`invert_x`、`invert_y`、`offset_x`、`offset_y`。
`Screen_TouchReadRaw()` 返回未换算的原生坐标，便于对照标定。
方向 2/3（横屏）的触摸换算是按 MADCTL 的 `MV` 位推出来的，实物核对前请先只在方向 0 下用。

## 与移植前工程的差异（除板卡外）

| 项 | arduino-uno-r4-wifi/P169H002 | 本工程 | 原因 |
|---|---|---|---|
| 总线后端 | `src/screen/bus/spi_dma_ra4m1.*`（SPI0 + DTC） | `src/screen/bus/lcd_spi_stm32f4.*`（SPI1 + DMA2） | 换芯片，接口契约（Begin/Write/LastError/ActualHz）不变 |
| 后端名 | `ScreenSpiDma_*` | `LcdSpi_*` | 名字不再绑定某个芯片的搬运器 |
| 后端编译开关 | `SCREEN_SPI_DMA && defined(ARDUINO_ARCH_RENESAS_UNO)` | `SCREEN_SPI_DMA && defined(STM32F410Rx)` | 主机端测试没有该宏，自动只编译逐字节回退路径 |
| 自检行 | `tx=DTC ... actual=24.0MHz` | `tx=DMA ... actual=20.0MHz` | 后端与真实时钟都变了 |
| **初始化顺序** | 先 `Wire.begin()`/触摸，再 `SPI.begin()` | 先 `SPI.begin()` + 面板初始化，最后初始化触摸 | 移植前是为了绕开 RA4M1 `IRQManager` 只有 32 个可编程向量的限制（Wire 与 SPI+DTC 各要 4 个）。F410 用 HAL 中断，没有这个限制，因此直接用样板的顺序 |
| **CST816 复位流程** | `Wire.end()` -> 复位芯片 -> `Wire.begin()`/`setClock()`（再等 100 ms） | `Wire` 由 `Screen_Init()` 起好，驱动只复位 + 等 200 ms | 同上，RA4M1 专有（复位后 RX 完成事件不再到达的绕法）；F410 走硬件 I2C1，200 ms 唤醒等待保留 |
| 标题文字 | `UNO R4 WiFi` | `Nucleo F410RB` | 只换板名，字号/位置/颜色不变 |
| 背光断言 | “外部硬件保持 255、D6 全程不被固件触碰” | “初始化拉低、setup 后拉高，全程只碰 3 次” | 这块板子的背光由固件驱动 |
| `demo_app.h` | 无 `extern "C"` | 包 `extern "C"` | `Core/Src/main.c` 用 C 直接调 `DemoApp_Setup()/Loop()` |

面板表（16 条初始化命令、四个方向的 `0x36`、`offset_x/offset_y`）、`gfx/`、`assets/logo.h`、
`fonts.*`、`demo_app.cpp` 的画面与文字、`touch/*` 与源工程逐字一致
（仅 `touch_cst816.cpp` 去掉了上面那一处 RA4M1 条件块）。

参考工程 `lcd_init.c` 的 `#if USE_HORIZONTAL` 分支取 `USE_HORIZONTAL==0` 的值（`0x36 = 0x00`）。
`offset_x[4] / offset_y[4]` 也照抄参考工程 `LCD_Address_Set()`：方向 0 行地址 +20、方向 1 行地址 +80、
方向 2 无偏移、方向 3 列地址 +80。

## 验证

### 已执行

1. 主机端行为验证（`sh test/run.sh`，g++ 编译 `src/screen` + `src/app` 并喂给虚拟 ST7789 240x280）：
   **全部通过（失败 0 项，65 项检查）**，覆盖
   - 初始化 16 条命令（`0x11 0x36 0x3A 0xB2 0xB7 0xBB 0xC2 0xC3 0xC4 0xC6 0xD0 0xE0 0xE1 0x21 0x29`
     + 末尾 MADCTL）、各命令数据长度、`0x3A=0x05`、`0x11` 后 120 ms 延时；
   - 背光脚 D6：初始化只碰 2 次（pinMode + 拉低），setup 后拉高，整个演示期间无多余操作；
   - 触摸芯片 ID 读回 0xB5、面板像素格式 RGB565、分辨率 240x280；
   - LOGO 展开 52 800 像素（= 240x220）@ (0,29)、GRAM (0,49)（方向 0 的 +20 偏移）、四角黑白正确；
   - 24/16/32 号三段文字的笔画像素，5 个纯色、6 条 40 像素彩条、24 级灰阶（每格 = `RGB565(n*255/23)`）；
   - 手写笔画、Clear 按钮（灰底 + 16 号黑字）、点按钮后不清屏；
   - 触摸坐标在方向 0/1/2/3 下的换算（含 `offset_y = 5` 生效）、`Screen_TouchReadRaw()` 未换算、抬手判定；
   - GRAM 偏移：`0x2A/0x2B` 起始地址在方向 0/1/3 下分别是 (0,20)、(0,80)、(80,0)，方向 2 无偏移。

   结果图 `test/out/*.png` 可人工核对：`1-logo.png`（LOGO）、`2-text.png`（文字）、
   `3-red.png`（纯色）、`4-colorbar.png`（彩条）、`5-grayscale.png`（灰阶）、`6-handwriting.png`（手写）、
   `7-button-ignored.png`（点按钮后仍不清屏），`8-rotation0.png` 是偏移测试后整屏填黑的结果。
2. Keil 实机工程编译（Arm Compiler 6.22）：
   `Program Size: Code=24216 RO-data=29840 RW-data=56 ZI-data=13712`，0 error / 0 warning，
   已生成 `*.axf` 与 `*.hex`。

### 需要实物确认（未做）

- SPI/I2C 实际波形与 20 MHz 时钟裕量，以及 24 MHz 请求档下是否出现随机色点/花屏
  （出现时把板级 `SCREEN_SPI_HZ` 降到 `12000000UL` 复查）；
- **CST816 的芯片 ID 与真实触摸坐标**：只有串口读到非零芯片 ID 并报出真实坐标才算触摸通过，
  软件不会用硬编码 ID 或假坐标绕过；触摸的 5 像素 Y 偏移与四个方向的方向性同样需要实物核对；
- ST7789 实际显色、Gamma 与 240x280 可见区的四周偏移；
- 背光电流与亮度；
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
  不代表规格书亮度；若转接板上仍留着 UNO R4 时期的背光飞线，固件能拉高但拉不低，
  要调光必须先拆掉飞线。
- 整屏填充 240x280 需要发 134 400 字节，20 MHz 下纯线路传输下限约 54 ms，
  纯色/灰阶画面上能看到自上而下的推进。
- 字符宽度按 `sizey/2`，超出屏幕的字模整字丢弃（不会写出窗口外）。
- `test/out/*.png` 里白色偏 `(248,252,248)`：RGB565 每通道截到 5/6/5 位后按位展开，是该格式的正常行为。

## 资产来源

- `Drivers/`：STM32Cube_FW_F4_V1.28.3（BSD-3-Clause）；
- `src/screen/gfx/fonts.{h,cpp}`：参考工程 `HARDWARE/LCD/lcdfont.h`，只把中文索引从 GBK
  改成 UTF-8 三字节，点阵数据未动；
- `src/screen/assets/logo.h`：参考工程 `HARDWARE/LCD/pic.h` 的 `gImage_logo`（240x220，RGB565，
  前 8 字节是图像头），用 `tools/pic2rle.py` 转成 RLE；参考工程的 `LCD_ShowPicture()` 把这 8 字节
  当像素一起发出（整图右移 4 像素、末尾少 4 像素），本工程从第 8 字节起完整显示：

```sh
python tools/pic2rle.py <参考工程>/HARDWARE/LCD/pic.h 240 220 src/screen/assets/logo.h --skip 8 --bpp 2
```

- `src/screen`、`src/app`、`test/host`：移植自 `arduino-uno-r4-wifi/P169H002`
  （其自身又来自 `stm32g0b1rc-P169H002`）；
- `MDK-ARM/startup_stm32f410rx.s`、工程骨架：STM32Cube_FW_F4 的 Nucleo-F410RB 模板。
