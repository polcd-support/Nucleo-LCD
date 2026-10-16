/*
 * 演示程序：复刻参考工程 Core/Src/lcd_demo.c 的 LCD_DEMO()。
 *
 * 参考工程是“初始化后 while(1) 跑一整段序列”，不是状态机；这里把这段序列拆成
 * 一个循环状态机（LOGO -> TEXT -> COLOR_FULL x5 -> COLOR_BAR -> GRAYSCALE -> 回到 LOGO），
 * 时间参数与参考工程 lcd.h 的宏一致（3000/2000/1000/3000/3000）。
 *
 * 与参考工程的一处实现差异（视觉结果相同）：参考工程每轮循环都重复整屏重绘，
 * 这里改为进入状态时画一次，避免 RLE LOGO / 灰阶在阻塞式循环里反复重画。
 *
 * 本模组无触摸：参考工程的 demo 也是纯显示循环，不响应触摸、不画清屏按钮。
 *
 * 入口：Core/Src/main.c 的 main() 直接调用 DemoApp_Setup() / DemoApp_Loop()
 * （相当于 Arduino 草图里的 setup() / loop()），因此这里用 extern "C" 链接。
 */
#ifndef DEMO_APP_H
#define DEMO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化屏幕、清屏、开背光，并进入 LOGO 画面。 */
void DemoApp_Setup(void);

/* 在 loop() 里反复调用，每次推进一小步。 */
void DemoApp_Loop(void);

#ifdef __cplusplus
}
#endif

#endif
