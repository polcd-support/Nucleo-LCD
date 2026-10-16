/*
 * 屏幕模块编译期配置。
 *
 * 换屏只改这里：选板子、选屏幕（面板 + 触摸）、选默认方向。
 * 新增屏幕的步骤见 README。
 */
#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

/* ---------------- 板卡 ---------------- */
#define SCREEN_BOARD_NUCLEO_F410RB 1
#define SCREEN_BOARD SCREEN_BOARD_NUCLEO_F410RB

/* ---------------- 屏幕 ---------------- */
/* 设备 = 面板 + 触摸组合，注册表见 screen_devices.h。 */
#define SCREEN_DEVICE_P035M075_IPS_CTP 0
#define SCREEN_DEVICE SCREEN_DEVICE_P035M075_IPS_CTP

/* ---------------- 触摸驱动 ---------------- */
/* P035M075 模组规格书第 2 页为 FT6336U（FT6xxx 寄存器，兼容 FT6236）；
 * CST816 供使用同款 FPC 触摸排线的其它模组选择。 */
#define SCREEN_TOUCH_DRIVER_FT6336U 0
#define SCREEN_TOUCH_DRIVER_CST816 1
#define SCREEN_TOUCH_DRIVER SCREEN_TOUCH_DRIVER_FT6336U

/* ---------------- 运行参数 ---------------- */
/* 上电方向：0/1 竖屏，2/3 横屏（相对面板原生方向）。 */
#define SCREEN_DEFAULT_ROTATION 0

/* 发送缓冲区字节数，必须同时被 2（RGB565）和 3（RGB666）整除：
 * 5760 = ILI9488 竖屏 6 行 RGB666（1920 像素 x 3 字节）= 2880 像素 x 2 字节。
 * F410RB 有 32 KB RAM，与 UNO R4 工程保持同一个值，便于两条线对拍。 */
#define SCREEN_TX_BYTES 5760

/* FT6336U 触摸阈值：数值越低越灵敏；128 偏钝，先用 40。 */
#define SCREEN_TOUCH_THRESHOLD 40

/* 1 = 初始化时通过串口 115200 打印面板名、触摸 ID 和结果。 */
#define SCREEN_DEBUG 1

/* ---------------- 板卡引脚映射 ---------------- */
#include "boards/nucleo_f410rb.h"

#endif
