#ifndef __APP_SENSOR_H
#define __APP_SENSOR_H

#include "stm32f10x.h"

/* ============ 温湿度采集业务 ============
 * 传感器不能每帧都读（1 次要阻塞 80ms+，会卡死刷新），所以做成"1 秒采一次、
 * 缓存结果供别处取用"的发布-订阅模式：任何模块都能随时调 GetXX 拿最新值。 */

void AppSensor_Init(void);            /* 初始化传感器 */

/* 周期推进：main 每 1 秒调用一次，触发一次 AHT20 读取并更新缓存 */
void AppSensor_Tick(void);

/* 查询最新值（无参直接读，内部是全局缓存） */
uint16_t AppSensor_GetTempX10(void);  /* 温度×10：25.5℃ → 255 */
uint16_t AppSensor_GetHumX10(void);   /* 湿度×10：60.0% → 600 */
void     AppSensor_DebugPrint(void);  /* 调试：printf 打一行（调试完可删） */

#endif