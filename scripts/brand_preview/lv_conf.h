#ifndef LV_CONF_H
#define LV_CONF_H

/* Host-only configuration: never included by the firmware. */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (8 * 1024 * 1024U)
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_MEM_INTEGRITY 1
#define LV_USE_ASSERT_OBJ 1
#define LV_USE_ASSERT_STYLE 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif
