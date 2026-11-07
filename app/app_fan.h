#ifndef __APP_FAN_H
#define __APP_FAN_H

#include "stm32f10x.h"

/* ================= 风扇业务状态机：关 / 运行 =================
 * - "档位"就是"占空比":0~100，无级。开着时档位=占空比% 。
 * - 启动突发：电动机低速起不来(S8050 那一路也一样)，所以每次档位变化
 *   或重新开机都先满占空比轰 100ms 再落到目标占空比(立项风险 1)。
 * 本模块只认"是谁"，不碰具体硬件——它调 PWM_SetDuty 而已。 */

void AppFan_Init(void);            /* 初始化：关状态，PWM 置 0 */

/* ---- 主循环处理按键事件后调用的动作 ---- */
void AppFan_On(void);              /* 开机：回到上次档位(≥1) */
void AppFan_Off(void);             /* 关机：PWM=0 进关态 */
void AppFan_Toggle(void);          /* 开/关切换 */
void AppFan_LevelUp(void);         /* 档位 +1（若关机则自动开机） */
void AppFan_LevelDown(void);       /* 档位 -1（减到 0 即关机转 OFF 态） */

/* ---- 查询 ---- */
uint8_t AppFan_IsRunning(void);    /* 1=运行态 */
uint8_t AppFan_GetLevel(void);     /* 当前档位 0~100 */
uint16_t AppFan_GetDutyCondition(void); /* 1=正在启动突发(给显示/上报用，可不写) */

/* ---- 周期推进：每 1ms 调一次，处理启动突发倒计时并写 PWM ---- */
void AppFan_Tick(void);

#endif