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
#define SCREEN_DEVICE_P0147B0108 0
#define SCREEN_DEVICE SCREEN_DEVICE_P0147B0108

/* ---------------- 触摸驱动 ---------------- */
/* 本模组无触摸（UNO R4 源工程没有 src/screen/touch/，参考工程 CST816 也是空目录，
 * demo 只用 SPI 点屏），选 NONE：设备表里 touch 为 NULL，没有任何触摸驱动参与编译，
 * 本工程不带 src/screen/touch/。 */
#define SCREEN_TOUCH_DRIVER_NONE 0
#define SCREEN_TOUCH_DRIVER SCREEN_TOUCH_DRIVER_NONE

/* ---------------- 运行参数 ---------------- */
/* 上电方向：0/1 竖屏，2/3 横屏（相对面板原生方向）。 */
#define SCREEN_DEFAULT_ROTATION 0

/* 发送缓冲区字节数，必须同时被 2（RGB565）和 3（RGB666）整除：
 * 5760 = 2880 像素 x 2 字节 = 1920 像素 x 3 字节。
 * F410RB 有 32 KB RAM，与 UNO R4 工程保持同一个值，便于两条线对拍。 */
#define SCREEN_TX_BYTES 5760

/* 1 = 初始化时通过串口 115200 打印面板名、触摸 ID 和结果。 */
#define SCREEN_DEBUG 1

/* ---------------- 板卡引脚映射 ---------------- */
#include "boards/nucleo_f410rb.h"

#endif
