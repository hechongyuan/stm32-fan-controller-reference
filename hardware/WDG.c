/* ============================================================================
 * WDG.c —— 独立看门狗 IWDG（程序跑飞自动复位）
 * ----------------------------------------------------------------------------
 * 原理：IWDG 由芯片内部 LSI(~40kHz) 独立时钟驱动，不受主时钟影响。
 *       计数器从你设的重装值递减到 0 时 → 触发整机复位。
 *       程序只要每隔一段时间喂狗（把计数拉回满值）就没复位；
 *       一旦程序跑飞、主循环喂不到狗，就在超时后自动重启。
 *
 * 参数算账：超时时间 = 预分频 × (重装值+1) / LSI
 *   LSI≈40kHz，选预分频 64 → 计数节拍 40k/64 = 625Hz
 *   重装值 1953 → 超时 ≈ 1953/625 ≈ 3.1 秒
 * 主循环每 100ms 喂一次，10 倍余量，够宽也够灵。
 * ========================================================================== */
#include "WDG.h"

void WDG_Init(void)
{
    /* 调试友好（可选）：Keil 仿真/Halt 时冻结看门狗，断点调试才不会一直复位。
     * 需要 stm32f10x_conf.h 里包含 dbgmcu 才可用；不用就整段删掉。
     * RCC_APB2PeriphClockCmd(RCC_APB2Periph_DBGMCU, ENABLE);
     * DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE); */

    /* IWDG 的关键寄存器带写保护，必须先解除写保护才能改 */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    IWDG_SetPrescaler(IWDG_Prescaler_64);    /* 预分频 64 */
    IWDG_SetReload(1953);                    /* 重装值 → 超时约 3.1s */
    IWDG_ReloadCounter();                    /* 把装载值先预置进计数器 */
    IWDG_Enable();                           /* 启动看门狗（一旦开，关不掉） */
}

void WDG_Feed(void)
{
    IWDG_ReloadCounter();                    /* 把计数器重新拉满，别让它饿死 */
}