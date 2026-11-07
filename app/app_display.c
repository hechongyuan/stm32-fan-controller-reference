/* ============================================================================
 * app_display.c —— 显示业务
 * ----------------------------------------------------------------------------
 * 两页内容：
 *   状态页(page 0)：风扇状态 + 档位 + 温度 + 湿度   （使用者最常看）
 *   参数页(page 1)：协议校验 + 版本 + 看门狗          （演示/调试用）
 * 每 100ms：先查 app_fan / app_sensor 的最新数据画进显存缓冲，再一次性刷屏。
 * 全部字符都是 8x8，一行 16 个字，竖着 4 行 —— 排版按这个格子思路来。
 * ========================================================================== */
#include "app_display.h"
#include "OLED.h"
#include "app_fan.h"
#include "app_sensor.h"

static uint8_t g_page = PAGE_STATUS;   /* 当前显示哪页 */

void AppDisplay_Init(void)
{
    OLED_Init();                        /* 屏幕初始化(含清屏) */
    g_page = PAGE_STATUS;
}

void AppDisplay_SetPage(uint8_t page)
{
    if (page > PAGE_PARAM) page = PAGE_PARAM;   /* 防御：越界钳住 */
    g_page = page;
}

/* 两页间翻页：状态页↔参数页（0→1, 1→0），KEY1 长按触发 */
void AppDisplay_NextPage(void)
{
    g_page = (g_page == PAGE_STATUS) ? PAGE_PARAM : PAGE_STATUS;
}

uint8_t AppDisplay_GetPage(void)
{
    return g_page;
}

/* 每 100ms：重画当前页 → 上屏 */
void AppDisplay_Tick(void)
{
    /* 先准备好本帧要展示的数据（取当次最新值，一次取齐，避免中途变） */
    uint8_t   level = AppFan_GetLevel();
    uint8_t   run   = AppFan_IsRunning();
    uint16_t  temp  = AppSensor_GetTempX10();    /* 25.5℃ → 255 */
    uint16_t  humi  = AppSensor_GetHumX10();     /* 60.0% → 600 */

    if (g_page == PAGE_STATUS)          /* ============ 状态页 ============ */
    {
        /* 第一行：风扇开关状态 */
        if (run) OLED_ShowStr(0, 0, "FAN   ON");
        else     OLED_ShowStr(0, 0, "FAN   OFF");

        /* 第二行：档位/占空比 DUTY:  78% (3 位定宽对齐) */
        OLED_ShowStr(1, 0, "DUTY:");
        OLED_ShowNum(1, 7, level, 3);
        OLED_ShowStr(1, 10, "%");

        /* 第三行：温度 TMP: 25.5C */
        OLED_ShowStr(2, 0, "TMP:");
        if (temp > 0)                   /* 传感器正常才有数 */
        {
            OLED_ShowFloat1(2, 7, temp);
            OLED_ShowStr(2, 11, "C");
        }
        else
            OLED_ShowStr(2, 7, "N/A  ");

        /* 第四行：湿度 HUM: 60.0% */
        OLED_ShowStr(3, 0, "HUM:");
        if (humi > 0)
        {
            OLED_ShowFloat1(3, 7, humi);
            OLED_ShowStr(3, 11, "%");
        }
        else
            OLED_ShowStr(3, 7, "N/A  ");
    }
    else                                /* ============ 参数页 ============ */
    {
        OLED_ShowStr(0, 0, "SYS  PARA");
        OLED_ShowStr(1, 0, "CRC :  OK");     /* 协议帧校验方式(见 app_protocol) */
        OLED_ShowStr(2, 0, "VER : 1.0");
        OLED_ShowStr(3, 0, "WDOG: ON ");
    }

    OLED_Update();                      /* 缓冲画完，一次性刷上屏 */
}