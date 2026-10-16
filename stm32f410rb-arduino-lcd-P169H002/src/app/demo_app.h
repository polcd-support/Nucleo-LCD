/*
 * 演示程序：与参考工程 Core/Src/lcd_demo.c 的 LCD_DEMO() 效果一致（P169H002 版）。
 *
 * 状态顺序（参考工程里可到达的流程）：
 *   LOGO 3s -> 文字 2s -> 全屏纯色 5 色 x 1s -> 彩条 3s -> 灰阶 3s -> 手写 + 清屏按钮
 *
 * 与参考工程的三处实现差异（视觉结果相同）：
 *  - 参考工程在每个循环里重复整屏重绘；这里改为进入状态时画一次，
 *    否则 RLE LOGO 和灰阶每 10 ms 重画一次会让触摸失去响应。
 *  - 参考工程里 STATE_IMAGE / STATE_COUNTDOWN 不可达，这里没有移植。
 *  - 参考工程手写页的按钮命中检测被注释掉（只画不响应），这里同样只画不响应。
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
