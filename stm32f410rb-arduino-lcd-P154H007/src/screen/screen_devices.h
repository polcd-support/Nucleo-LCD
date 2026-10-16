/*
 * 屏幕设备注册表：新增屏幕时在这里加一行。
 *
 * 面板头文件都包含 screen_config.h 与自身的 #if 守卫，未选中的设备不会参与编译。
 */
#ifndef SCREEN_DEVICES_H
#define SCREEN_DEVICES_H

#include "screen_config.h"
#include "panels/panel_p154h007.h"

static const ScreenDevice *const SCREEN_DEVICE_TABLE[] = {
    &Screen_Device_P154H007,
};

#define SCREEN_DEVICE_COUNT (sizeof(SCREEN_DEVICE_TABLE) / sizeof(SCREEN_DEVICE_TABLE[0]))

#endif
