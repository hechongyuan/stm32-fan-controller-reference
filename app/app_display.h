#ifndef __APP_DISPLAY_H
#define __APP_DISPLAY_H

#include "stm32f10x.h"

/* ============ 显示业务：两页 OLED（状态页 / 参数页） ============
 * - 只负责"把哪些数据画到哪"，刷新节奏由 main 用 100ms 周期调 Tick。
 * - 画进显存缓冲，一次性 OLED_Update() 上屏（不闪烁、不抢总线）。 */

/* 页号 */
#define PAGE_STATUS   0   /* 状态页：风扇/档位/温湿度 */
#define PAGE_PARAM    1   /* 参数页：协议/版本/看门狗 */

void AppDisplay_Init(void);        /* 初始化：默认状态页，清屏 */
void AppDisplay_SetPage(uint8_t page);   /* 指定切到某页 */
void AppDisplay_NextPage(void);    /* 在 状态页↔参数页 之间翻页（KEY1 长按调用） */
uint8_t AppDisplay_GetPage(void);  /* 当前在第几页 */
void AppDisplay_Tick(void);        /* 每 100ms 调：重绘 + 上屏 */

#endif