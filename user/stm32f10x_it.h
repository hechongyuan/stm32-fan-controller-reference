#ifndef __STM32F10X_IT_H
#define __STM32F10X_IT_H

/* 本工程用到的全部中断服务函数的声明都在这里（Keil 工程按此文件名字约定）。
 * 内容与模板同名文件一致，只是函数体换成了我们的实现。 */

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void USART1_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI15_10_IRQHandler(void);

#endif