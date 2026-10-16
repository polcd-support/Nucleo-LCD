/*
 * 演示程序：与参考工程 Core/Src/lcd_demo.c 的 LCD_Display_RunTest() 效果一致。
 *
 * 参考工程流程：
 *   LCD_Init() -> 全屏黑 -> 开背光 -> 死循环 { 彩条 3s -> 灰阶 3s }
 *
 * 本屏为纯显示（无触摸、无文字、无图片），状态机只有彩条与灰阶两个状态，来回切换。
 * 与参考工程的一处实现差异（视觉结果相同）：
 *  - 参考工程在每个循环里重复整屏重绘；这里改为进入状态时画一次，
 *    避免每 10 ms 重画拖慢主循环。
 *
 * 入口：Core/Src/main.c 的 main() 直接调用 DemoApp_Setup() / DemoApp_Loop()
 * （相当于 Arduino 草图里的 setup() / loop()），因此这里用 extern "C" 链接。
 */
#ifndef DEMO_APP_H
#define DEMO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化屏幕、清屏、开背光，并进入彩条画面。 */
void DemoApp_Setup(void);

/* 在 loop() 里反复调用，每次推进一小步。 */
void DemoApp_Loop(void);

#ifdef __cplusplus
}
#endif

#endif
