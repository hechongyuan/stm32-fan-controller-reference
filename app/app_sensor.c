/* ============================================================================
 * app_sensor.c —— 温湿度采集业务（1 秒周期）
 * ----------------------------------------------------------------------------
 * 读 AHT20 是"阻塞 80ms + 忙 I2C"的活，绝不能放在 1ms/100ms 的快速循环里，
 * 否则 OLED 每 100ms 刷一帧时会被它卡到 180ms+。
 * 所以本层把"采集"和"展示"解耦：
 *   采集：main 每 1s 调一次 AppSensor_Tick()（低速、阻塞，无所谓）；
 *   展示：其他模块随时调 GetXX()（高速、无阻塞）。
 * 缓存值就两个全局变量，只在本文件内改，别处只读（信息隐藏）。
 * ========================================================================== */
#include "app_sensor.h"
#include "AHT20.h"
#include <stdio.h>   /* printf 用 */

static volatile uint16_t g_tempX10 = 0;   /* 缓存：最近一次读到的温度×10 */
static volatile uint16_t g_humX10  = 0;   /* 缓存：最近一次读到的湿度×10 */
static volatile uint8_t  g_ready   = 0;   /* 传感器第一次成功读数前为 0 */

void AppSensor_Init(void)
{
    AHT20_Init();            /* 上电校准（失败也继续，后面每 1s 重试覆盖） */
}

/* main 每 1s 调用一次 */
void AppSensor_Tick(void)
{
    uint16_t t = 0, h = 0;
    if (AHT20_Read(&t, &h) == 0)         /* 读成功才更新缓存 */
    {
        g_tempX10 = t;
        g_humX10  = h;
        g_ready   = 1;
    }
    /* 读失败：保留上次数据不动，等下一秒再试（别让屏幕乱跳） */
}

uint16_t AppSensor_GetTempX10(void)
{
    return g_ready ? g_tempX10 : 0;      /* 没读成功过就报 0(界面显示 N/A) */
}

uint16_t AppSensor_GetHumX10(void)
{
    return g_ready ? g_humX10 : 0;
}

void AppSensor_DebugPrint(void)
{
    printf("TMP=%d.%d HUM=%d.%d\r\n",
           g_tempX10 / 10, g_tempX10 % 10,
           g_humX10  / 10, g_humX10  % 10);
}