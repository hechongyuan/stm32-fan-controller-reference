/* ============================================================================
 * PWM.c —— 风扇 PWM 输出驱动
 * ----------------------------------------------------------------------------
 * PA0 的功能是 TIM2_CH1（复用推挽），输出一个 1kHz 的 PWM 波。占空比由
 * app 层通过 PWM_SetDuty 设置，最终经 1kΩ 电阻到 S8050 基极做低边开关，
 * 高电平时间越长 → 风扇通电越多 → 转速越高（无级调速）。
 *
 * 参数怎么算：72MHz 时钟 → 预分频 71（分 72 倍）→ 1MHz → 计数值 0~999
 *             → 1MHz / (999+1) = 1kHz  PWM 频率
 * 占空比 = 脉宽/周期 = CCR/(ARR+1)，所以 duty 1% 就对应 CCR = 10。
 * ========================================================================== */
#include "PWM.h"

void PWM_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /* ---- 1. 开时钟：PA0 在 GPIOA，PWM 用 TIM2（APB1 外设） ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* ---- 2. PA0 配成"复用推挽输出"（复用 = 交给定时器控制引脚） ---- */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ---- 3. 定时器时基：预分频 72 → 计数时钟 1MHz，ARR 999 → 1kHz ---- */
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;   /* 72MHz/72 = 1MHz */
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period            = 1000 - 1; /* 0~999，1MHz → 1kHz */
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* ---- 4. 输出比较通道 CH1：PWM 模式 1，初始占空比 0% ---- */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;               /* CCR1 = 0 → 脉宽 0 */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    /* PWM 占空比更新后要"预装载生效"，否则瞬时写入会撕裂波形 */
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

    /* ---- 5. 启动定时器 ---- */
    TIM_Cmd(TIM2, ENABLE);
}

/* 设置占空比：入参 0~100（百分数），换算成 CCR1 写入
 * 换算：周期=ARR+1=1000 个计数，所以 duty% → CCR = duty*10（50% → 500） */
void PWM_SetDuty(uint8_t duty)
{
    uint16_t ccr;
    if (duty > 100) duty = 100;                     /* 防御：超界就钳到 100 */
    ccr = (uint16_t)duty * 10u;                     /* 1% 占空比 = CCR 10 */
    TIM_SetCompare1(TIM2, ccr);
}