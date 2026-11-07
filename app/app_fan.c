/* ============================================================================
 * app_fan.c —— 风扇业务状态机
 * ----------------------------------------------------------------------------
 * 这是整个项目"含金量"最高的一层，面试就讲它：
 *   - 两个状态：关(OFF) 与 运行(RUN)
 *   - 档位即占空比（0~100 无级）
 *   - 启动突发：每次档位 "变大" 都先满占空比 100ms 再落目标，确保小马达能起步
 * 与硬件解耦：本文件完全不认识引脚，只知道"调 PWM_SetDuty(占空比)"，
 * 所以以后换电机、换 PWM 通道，这个文件一行不用改。
 * ========================================================================== */
#include "app_fan.h"
#include "PWM.h"

#define BURST_MS   100    /* 启动突发时长：满载 100ms */

typedef enum              /* 风扇状态机的两个状态 */
{
    FAN_STATE_OFF,        /* 关机：PWM=0 */
    FAN_STATE_RUN         /* 运行：PWM=档位（可能正走在突发里） */
} FanState_t;

static FanState_t g_state = FAN_STATE_OFF;
static uint8_t    g_level = 0;     /* 档位 0~100 */
static uint16_t   g_burst = 0;     /* 启动突发剩余 ms（0 表示不在突发） */

void AppFan_Init(void)
{
    g_state = FAN_STATE_OFF;
    g_level = 0;
    g_burst = 0;
    PWM_Init();                     /* 初始化 PWM；占空比先 0 */
    PWM_SetDuty(0);
}

/* 开机：回到上次档位（起码 1 档，否则无意义） */
void AppFan_On(void)
{
    g_state = FAN_STATE_RUN;
    if (g_level == 0)
        g_level = 1;                /* 开机默认最低 1 档 */
    g_burst = BURST_MS;             /* 一起步就轰一把（启动突发） */
}

void AppFan_Off(void)
{
    g_state = FAN_STATE_OFF;
    g_burst = 0;
    PWM_SetDuty(0);                 /* 立即停（不经过突发） */
}

void AppFan_Toggle(void)
{
    if (g_state == FAN_STATE_RUN)
        AppFan_Off();
    else
        AppFan_On();
}

/* 加档：如果当前是关机，先开机（按键直觉：调档就是想让扇子转起来） */
void AppFan_LevelUp(void)
{
    if (g_state == FAN_STATE_OFF)
        AppFan_On();
    if (g_level < 100)
        g_level++;
    g_burst = BURST_MS;             /* 档位一变，立刻重新惊动一次（防低速卡住） */
}

/* 减档：减到 0 就关机（方案B：风量调到 0 = 关闭，直观） */
void AppFan_LevelDown(void)
{
    if (g_state == FAN_STATE_OFF)   /* 关机状态下减档：先开机到 1 */
    {
        AppFan_On();
    }
    else if (g_level > 1)           /* 档位 > 1：正常减一档 */
    {
        g_level--;
        g_burst = BURST_MS;         /* 降档也来一下，防止刚好卡在静摩擦 */
    }
    else                            /* 档位 == 1：再减就到 0，转为关机 */
    {
        AppFan_Off();
    }
}

uint8_t AppFan_IsRunning(void)
{
    return (g_state == FAN_STATE_RUN) ? 1u : 0u;
}

uint8_t AppFan_GetLevel(void)
{
    return g_level;
}

uint16_t AppFan_GetDutyCondition(void)
{
    return g_burst;                 /* 若非 0 表示正处在启动突发（显示/调试用） */
}

/* 每 1ms 推进一次：突发倒计时结束后把 PWM 落到目标档位 */
void AppFan_Tick(void)
{
    if (g_state == FAN_STATE_RUN)
    {
        if (g_burst > 0)
        {
            g_burst--;              /* 突发中：倒计时 */
            PWM_SetDuty(100);       /* 满载输出，帮它起步 */
        }
        else
        {
            PWM_SetDuty(g_level);   /* 突发结束/无突发：按档位输出 */
        }
    }
}