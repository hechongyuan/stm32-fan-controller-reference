#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

/* 初始化风扇 PWM：PA0 = TIM2_CH1，频率 1kHz，初始占空比 0 */
void PWM_Init(void);

/* 设置占空比：duty = 0~100（百分数，内部换算成 CCR） */
void PWM_SetDuty(uint8_t duty);

#endif