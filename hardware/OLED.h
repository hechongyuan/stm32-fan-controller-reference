#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"
#include <stdint.h>

/* OLED 屏参数 */
#define OLED_WIDTH   128      /* 宽 128 像素 */
#define OLED_HEIGHT  64       /* 高 64 像素 */
#define OLED_PAGES   8        /* 64/8 行像素 = 8 个"页"(每页 8 行) */
#define OLED_CHAR_W  8        /* 字符宽 8 像素 */
#define OLED_CHAR_H  8        /* 字符高 8 像素 */
#define OLED_LINES   4        /* 8x8 字符，竖着能放 4 行 */

/* ---- 初始化：上电后调用一次，点亮屏幕并清屏 ---- */
void OLED_Init(void);

/* ---- 显存缓冲操作（先画进 RAM，最后一次性 OLED_Update 刷屏） ---- */
void OLED_Clear(void);                        /* 缓冲区清零（还没上屏） */
void OLED_ShowChar(uint8_t line, uint8_t col, char ch);           /* 第 line 行(0~3)、第 col 列(0~15)写一个字符 */
void OLED_ShowStr(uint8_t line, uint8_t col, const char *str);    /* 写字符串 */
void OLED_ShowNum(uint8_t line, uint8_t col, uint32_t value, uint8_t len);  /* 写整数：len=固定位数，高位补空格 */
void OLED_ShowFloat1(uint8_t line, uint8_t col, uint16_t x10);    /* 写"整数×10"的小数：x10=125 → 12.5 */
void OLED_Update(void);                       /* 把显存缓冲一次性刷到屏上 */

#endif