#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

/* 毫秒延时（阻塞，基于 SysTick 1ms 中断后的节拍轮询） */
void Delay_ms(uint32_t ms);

/* 全局毫秒时钟：上电 0 起，每 1ms 由中断 +1，可用来做"非阻塞计时" */
uint32_t G_GetMs(void);

/* 初始化 SysTick 为 1ms 中断节奏（在程序开头调用一次） */
void Delay_Init(void);

#endif