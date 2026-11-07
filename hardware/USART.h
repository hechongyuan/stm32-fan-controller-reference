#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>   /* 供下面 fputc 用 FILE */

/* 初始化 USART1：PA9=TX / PA10=RX，115200 8N1，接收中断(RXNE)打开 */
void USART_Init(void);

/* 发送：单字节 / 字符串 / 二进制数组（数组长度必须显式给，协议帧用它） */
void USART_SendByte(uint8_t byte);
void USART_SendStr(const char *str);
void USART_SendBuf(const uint8_t *buf, uint16_t len);

/* 接收（企业级做法：中断收进环形缓冲，主循环按需取走，不丢帧） */
void USART_RxISR(void);             /* 在 USART1_IRQHandler 里调用 */
uint16_t USART_RxAvailable(void);   /* 缓冲里积了几个字节 */
uint8_t  USART_RxReadByte(void);    /* 取出 1 字节（取走即少一个） */

/* 让 printf 输出到串口（Keil 的 C lib 会调 fputc） */
int fputc(int ch, FILE *f);

#endif