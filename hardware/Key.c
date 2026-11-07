/* ============================================================================
 * Key.c —— 双按键模块（KEY1=PB1 / KEY2=PB11）
 * ----------------------------------------------------------------------------
 * 职责分两半：
 *   A. 长短按判定（扫描法，非阻塞）：主循环每 10ms 喂一次 Key_ScanTick()。
 *      每颗键一个结构体，一按一瞬间只记三件事：cnt(按了多久)、hold_done
 *      (长按报过没=防连发)、event(待取事件)。
 *   B. 低功耗唤醒 EXTI：KEY1 配进 EXTI 线1、KEY2 配进 EXTI 线11，下降沿
 *      (上拉+按下拉低)。ISR 只负责"喊醒 CPU + 置唤醒标志"，数量级不做判断。
 *
 * 为什么"长短按判断"不放在中断里？中断要短。判长按要"连续观察 1 秒"，
 * 这是轮询/计时的活；EXTI 只管睡眠时把人叫醒。两者拼成一个模块。
 * ========================================================================== */
#include "Key.h"
#include "Delay.h"

/* 长按阈值：按住满 1 秒即判长按（配合 10ms 节拍：100 个 tick） */
#define KEY_LONG_TICKS   100

/* 每颗键的状态 */
typedef struct
{
    GPIO_TypeDef *port;     /* 所在端口 */
    uint16_t      pin;      /* 引脚号 */
    uint16_t      cnt;      /* 已按住几个 tick（0=没按） */
    uint8_t       hold_done;/* 本次按下，长按是否已上报（防按住连发重复报） */
    uint8_t       event;    /* 暂存待 app 取的事件 */
} Key_t;

static volatile uint8_t g_wake_flag = 0;   /* 唤醒标记：ISR 写、主循环读 → volatile */

/* 两颗键的定义：要换引脚只改这里 */
static Key_t g_keys[2] =
{
    {GPIOB, GPIO_Pin_1,  0, 0, KEY_NONE},   /* KEY1 = PB1 */
    {GPIOB, GPIO_Pin_11, 0, 0, KEY_NONE},   /* KEY2 = PB11 */
};

void Key_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* ---- 1. GPIO：两键配置成上拉输入（未按=高，按下接地=低） ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* ---- 2. AFIO：把引脚连到外部中断线（这一布不能漏，否则 EXTI 收不到） ---- */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource11);

    /* ---- 3. EXTI：线 1 和线 11，下降沿触发（睡眠时按键按下把 CPU 喊醒） ---- */
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;

    EXTI_InitStructure.EXTI_Line    = EXTI_Line1;
    EXTI_Init(&EXTI_InitStructure);

    EXTI_InitStructure.EXTI_Line    = EXTI_Line11;
    EXTI_Init(&EXTI_InitStructure);

    /* ---- 4. NVIC：开两个中断通道 ---- */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;

    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;        /* KEY1 的中断 */
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;    /* KEY2(线11) 的中断 */
    NVIC_Init(&NVIC_InitStructure);
}

/* ======================= 长短按扫描（每 10ms 调一次） ======================= */

void Key_ScanTick(void)
{
    uint8_t i;
    for (i = 0; i < 2; i++)
    {
        Key_t *k = &g_keys[i];
        uint8_t down = (GPIO_ReadInputDataBit(k->port, k->pin) == 0) ? 1 : 0;

        if (down)                               /* 正按着 */
        {
            if (k->cnt < 0xFFF0)
                k->cnt++;                       /* 按住时长 +1tick */

            if (k->cnt >= KEY_LONG_TICKS && k->hold_done == 0)
            {                                   /* 按满 1 秒、且还没报过长按 */
                k->hold_done = 1;               /* 锁住：按住不松不会重复报(防连发) */
                k->event = (uint8_t)KEY_LONG;   /* 立即报长按，不等松手 */
            }
        }
        else                                    /* 松开 */
        {
            if (k->cnt != 0)                    /* 确认之前按过 */
            {
                uint8_t was_long = k->hold_done;
                k->cnt = 0;
                k->hold_done = 0;               /* 复位，供下一次按下 */
                if (k->event == KEY_NONE && was_long == 0)
                    k->event = (uint8_t)KEY_SHORT;   /* 没到长按就松 → 短按 */
            }
        }
    }
}

uint8_t Key_Event(uint8_t idx)
{
    uint8_t ev;
    if (idx > 1) idx = 1;                       /* 防御：越界索引钳到合法 */
    ev = g_keys[idx].event;
    g_keys[idx].event = KEY_NONE;               /* 取走即清零，不取就一直攒着 */
    return ev;
}

/* ============================ EXTI 唤醒（M5） ============================ */

/* 由 stm32f10x_it.c 的 ISR 调用：line 传 EXTI_Line1 或 EXTI_Line11 */
void Key_Wake_ISR(uint8_t line)
{
    if (line == EXTI_Line1)
        EXTI_ClearITPendingBit(EXTI_Line1);     /* 清挂起标志(不请=一直进中断) */
    else
        EXTI_ClearITPendingBit(EXTI_Line11);
    g_wake_flag = 1;                            /* 只标记"我被按醒了" */
}

uint8_t Key_IsWake(void)
{
    return g_wake_flag;
}

void Key_ClearWake(void)
{
    g_wake_flag = 0;
}