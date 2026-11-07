#ifndef __WDG_H
#define __WDG_H

#include "stm32f10x.h"

/* 初始化独立看门狗 IWDG：超时约 3 秒（在全部初始化完成后调用一次）
 * 注意：IWDG 一旦使能就无法关闭，所以必须从主循环"按时喂狗" */
void WDG_Init(void);

/* 喂狗：主循环每 <100ms 调用一次（把计数器重置，防止超时复位） */
void WDG_Feed(void);

#endif