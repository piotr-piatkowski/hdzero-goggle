#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core/channel_table.h"
#include "ui/ui_main_menu.h"
#include <lvgl/lvgl.h>

#define HDZERO_CHANNEL_NUM (channel_set_size(g_setting.source.hdzero_channel_set))
#define ANALOG_CHANNEL_NUM 48

int scan(void);
int scan_reinit(void);
void autoscan_exit(void);
void page_scannow_set_channel_label(void);

extern page_pack_t pp_scannow;

#ifdef __cplusplus
}
#endif
