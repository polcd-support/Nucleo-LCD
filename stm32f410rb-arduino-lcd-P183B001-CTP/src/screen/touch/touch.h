/*
 * 触摸驱动入口。每个驱动提供 ScreenTouchDriver::init / read，
 * read 输出的是触摸面板原生坐标，方向修正由 ScreenTouchConfig 负责。
 */
#ifndef SCREEN_TOUCH_H
#define SCREEN_TOUCH_H

#include "../screen_config.h"
#include "../screen_device.h"

/* FT6336U：FT6xxx 寄存器，兼容 FT6236/FT6436。使用同款 FPC 排线的其它模组可选。 */
extern const ScreenTouchDriver Touch_Driver_FT6336U;

/* CST816S/T：0x15 地址，仅在选择该驱动时参与编译。 */
extern const ScreenTouchDriver Touch_Driver_CST816;

#endif
