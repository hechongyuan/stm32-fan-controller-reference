#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/* 按键事件码：主循环每 10ms 调 Key_ScanTick()，然后去 Key_Event() 拿事件 */
#define KEY_NONE    0    /* 没有事件 */
#define KEY_SHORT   1    /* 短按（按住时间 < 1s 就松手） */
#define KEY_LONG    2    /* 长按（按住 ≥ 1s，按满即报，不用等松手） */

#define KEY1        0    /* KEY1 = PB1 */
#define KEY2        1    /* KEY2 = PB11 */

/* 初始化：GPIO 上拉 + AFIO 接线 + EXTI 触发(用于低功耗唤醒) + NVIC */
void Key_Init(void);

/* 节拍：主循环每 10ms 调用一次，做消抖/长短按判断，事件暂存 */
void Key_ScanTick(void);

/* 取事件：传入 KEY1/KEY2，返回 KEY_NONE/KEY_SHORT/KEY_LONG，取走即清零 */
uint8_t Key_Event(uint8_t idx);

/* ---- 低功耗唤醒（M5）---- */
/* ISR 回调（由 stm32f10x_it.c 里的 EXTI1/EXTI15_10 中断调用）：
   只做"唤醒标记 + 清标志"，判断仍回主循环做。返回当前是否有唤醒待处理。 */
void Key_Wake_ISR(uint8_t line);     /* line：传 EXTI_Line1 / EXTI_Line11 */
uint8_t Key_IsWake(void);            /* 查询是否被按键唤醒过 */
void Key_ClearWake(void);            /* 主循环处理完唤醒后清除 */

#endif