/* ============================================================================
 * main.c —— 程序入口：初始化 → 分频调度 →（可选）低功耗
 * ----------------------------------------------------------------------------
 * 调度设计（超级循环 + 时间片，嵌入式最经典的组织方式）：
 *   全部"多久一次"都以系统毫秒时钟 G_GetMs() 为基准，主循环每圈看时钟
 *   够没够，分频执行不同周期的任务：
 *     每 1ms    : AppFan_Tick()   （启动突发的 100ms 倒计时需要 1ms 精度）
 *     每 10ms   : Key_ScanTick() + 按键事件分发 / 处理串口数据
 *     每 100ms  : OLED 刷新       （AppDisplay_Tick）
 *     每 1000ms : 采温湿度 + 串口上报一帧
 *     随时      : 喂看门狗（缝在 100ms 里）
 *   这样没有一个任务是"卡死其他任务"的阻塞等待。
 *
 * 低功耗(可选开)：风扇停 + 无按键活动 2s → 进 Stop，按键 EXTI 唤醒。
 *   注意与 IWDG 的冲突（见 README/第 6 节），调试阶段先关：
 * ------------------------------------------------------------------------- */
#include "stm32f10x.h"
#include "Delay.h"
#include "USART.h"
#include "WDG.h"
#include "Key.h"
#include "app_fan.h"
#include "app_display.h"
#include "app_sensor.h"
#include "app_protocol.h"

/* 0 = 调试阶段不睡（推荐）；1 = 功能全部稳定后打开低功耗入口 */
#define ENABLE_LOWPOWER   0

/* 低功耗触发条件：风机已停（关机或档位 0），且这么久没碰按键 */
#define LL_IDLE_MS        2000

int main(void)
{
    /* ---- 1. 分频任务的时间戳 ---- */
    uint32_t last1   = 0;    /* 1ms 任务 */
    uint32_t last10  = 0;    /* 10ms 任务 */
    uint32_t last100 = 0;    /* 100ms 任务 */
    uint32_t last1s  = 0;    /* 1s 任务 */
    uint32_t keyIdle = 0;    /* 距上次按键活动的时间戳（低功耗判断用） */

    /* ---- 2. 中断分组：整个工程统一设一次（按键=抢占2，串口=抢占1） ---- */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* ---- 3. 初始化（顺序有讲究：先时钟/延时，再驱动，再看门狗最后） ---- */
    Delay_Init();                 /* 1ms 系统节拍 */
    USART_Init();
    AppDisplay_Init();            /* 内部会初始化 OLED + 显示 */
    AppSensor_Init();             /* AHT20 校准 */
    AppFan_Init();                /* PWM + 风扇状态机 */
    Key_Init();                   /* 按键 + EXTI 唤醒配置 */
    AppProtocol_Init();           /* CRC8 查表 */
    WDG_Init();                   /* 看门狗最后开（开了就一路喂下去） */

    printf("FAN CTRL BOOT OK\r\n");

    /* ---- 4. 主循环（超级循环） ---- */
    while (1)
    {
        uint32_t now = G_GetMs();   /* 当前时刻(毫秒)，一次读完本轮共用 */

        /* ---------- 每 1ms：风扇状态机推进（启动突发倒计时） ---------- */
        if (now - last1 >= 1)  { last1 = now; AppFan_Tick(); }  /* 无符号减法不怕绕回 */

        /* ---------- 每 10ms：按键 + 串口命令 ---------- */
        if (now - last10 >= 10)
        {
            uint8_t ev1, ev2;
            last10 = now;

            Key_ScanTick();              /* 喂按键节拍(消抖/长短按) */

            /* 按键事件 → 业务动作（KEY1 短按=开关，长按=切页;KEY2 短=加档，长=减档） */
            ev1 = Key_Event(KEY1);
            if (ev1 == KEY_SHORT) { AppFan_Toggle();      keyIdle = now; }
            else if (ev1 == KEY_LONG) { AppDisplay_NextPage(); keyIdle = now; }

            ev2 = Key_Event(KEY2);
            if (ev2 == KEY_SHORT) { AppFan_LevelUp();    keyIdle = now; }
            else if (ev2 == KEY_LONG) { AppFan_LevelDown(); keyIdle = now; }

            AppProtocol_Process();       /* 解析上位机发来的控制帧 */
        }

        /* ---------- 每 100ms：OLED 刷新 + 喂狗 ---------- */
        if (now - last100 >= 100)
        {
            last100 = now;
            WDG_Feed();                  /* 看门狗：100ms 必喂(3s 才超时，余量足) */
            AppDisplay_Tick();           /* 重绘并刷屏 */
        }

        /* ---------- 每 1s：采集温湿度 + 上报一帧 ---------- */
        if (now - last1s >= 1000)
        {
            last1s = now;
            AppSensor_Tick();            /* 阻塞约 80ms(读 AHT20)，这里才允许慢 */
            AppProtocol_Tick();          /* 组帧 + 发送 */
        }

        /* ---------- 可选的低功耗入口（ENABLE_LOWPOWER=1 才进） ---------- */
#if ENABLE_LOWPOWER
        if (AppFan_IsRunning() == 0 &&      /* 风机已停 */
            now - keyIdle >= LL_IDLE_MS)    /* 且 2s 没碰按键 */
        {
            /* 预睡：先把狗喂满（Stop 期间主循环不跑，IWDG 由 LSI 独立计时仍计数，
             * 若睡眠超过超时会被复位——这是低功耗 × 看门狗的真实取舍点，
             * 产品里通常用 RTC 周期唤醒补喂或短睡。演示开 1s 以内按键立刻醒即可） */
            WDG_Feed();

            RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
            PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

            /* 从 Stop 醒来：时钟回到默认(HSI)，要重新切回 72MHz */
            SystemInit();

            WDG_Feed();                     /* 醒来第一件事喂狗 */
            keyIdle = G_GetMs();            /* 重置空闲计时，避免马上又睡 */
            /* 按键唤醒后，主循环照常跑；Key 的长短按判定由 10ms 节拍接管 */
        }
#endif
    }
}