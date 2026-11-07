/* ============================================================================
 * AHT20.c —— 温湿度传感器驱动（软件 I2C，地址 0x38，与 OLED 共用 PB8/PB9 总线）
 * ----------------------------------------------------------------------------
 * 协议要点（AHT20 没有寄存器寻址，全都是"命令流"）：
 *   1. 初始化：先发 0x71 读状态字，看 Bit3(校准使能)是否为 1；
 *      不是 1 才发 0xBE + {0x08,0x00} 初始化，等 10ms；
 *   2. 触发测量：写 0xAC + {0x33, 0x00}；
 *   3. 等 ≥80ms（内部测量中，此时不响应）；
 *   4. 读 6 字节：[0]=状态字节 [1..2]=湿度20bit [3..5]=温度20bit。
 *
 * 状态字位定义（Bit7=BUSY 测量中 / Bit3=校准使能，别搞混）：
 *   测量中判定看 bit7(0x80)；初始化判定看 bit3(0x08)。
 *
 * 20bit 拼接与换算（面试常问）：
 *   湿度RH%   = hum_code / 2^20 * 100           (hum_code 20 位)
 *   温度T℃   = temp_code / 2^20 * 200 - 50
 * 工程用整数运算规避浮点：乘除用"移位"代替 /2^20，最终输出整数×10。
 * ========================================================================== */
#include "AHT20.h"
#include "I2C_Soft.h"
#include "Delay.h"

#define AHT20_ADDR   0x38      /* 器件地址 7bit：0x38 */
#define AHT20_OK     0         /* 函数成功状态码 */
#define AHT20_ERR    1         /* 函数失败状态码 */

#define AHT20_STATUS_BUSY        0x80u   /* 状态字 Bit7：测量中 */
#define AHT20_STATUS_CALIBRATED  0x08u   /* 状态字 Bit3：已校准 */

uint8_t AHT20_Init(void)
{
    uint8_t status = 0;
    uint8_t cmd[2] = {0x08, 0x00};

    Delay_ms(50);                              /* 上电稳定（手册要求 ≥40ms） */

    /* ① 读回状态字（1 字节）——先检查再决定要不要初始化
     * 手册流程"发送 0x71"里的 0x71 = 设备地址(0x38)左移1位 + 读位，
     * 即起始后直接发"地址+读"。AHT20 没有寄存器地址，所以用 ReadRaw
     * 直读即可（ReadRegs 那种先写"寄存器"再读的套路对它是多余的）。 */
    if (I2C_Soft_ReadRaw(AHT20_ADDR, &status, 1) != 0)
        return AHT20_ERR;                      /* 设备没应答 = 没接好 */

    /* ② Bit3=1 表示已校准，直接可用，不重复初始化 */
    if (status & AHT20_STATUS_CALIBRATED)
        return AHT20_OK;

    /* ③ 未校准 → 发初始化命令 0xBE + {0x08,0x00}，等 10ms */
    if (I2C_Soft_WriteBuf(AHT20_ADDR, 0xBE, cmd, 2) != 0)
        return AHT20_ERR;
    Delay_ms(10);

    return AHT20_OK;
}

uint8_t AHT20_Read(uint16_t *tX10, uint16_t *hX10)
{
    uint8_t data[6];
    uint8_t cmd[2] = {0x33, 0x00};
    uint32_t hum_code, temp_code;

    /* ---- 1. 触发一次测量 ---- */
    if (I2C_Soft_WriteBuf(AHT20_ADDR, 0xAC, cmd, 2) != 0)
        return AHT20_ERR;

    Delay_ms(80);                              /* 等测量完成（≥75ms 才行） */

    /* ---- 2. 读 6 字节原始结果（AHT20 无寄存器地址，直接裸读） ---- */
    if (I2C_Soft_ReadRaw(AHT20_ADDR, data, 6) != 0)
        return AHT20_ERR;

    if (data[0] & AHT20_STATUS_BUSY)           /* 状态字 Bit7=BUSY=还在测量 */
        return AHT20_ERR;                      /* 数据不可用，直接放弃 */

    /* ---- 3. 拼出两个 20bit 原始值 ---- */
    hum_code  = ((uint32_t)data[1] << 12)        /* 湿度高 8 位 */
              | ((uint32_t)data[2] << 4)         /* 湿度中 8 位 */
              | ((uint32_t)data[3] >> 4);        /* 湿度低 4 位（温度高 4 位的邻居） */

    temp_code = ((uint32_t)(data[3] & 0x0F) << 16)  /* 温度高 4 位（同字节低 4 位） */
              | ((uint32_t)data[4] << 8)            /* 温度中 8 位 */
              | ((uint32_t)data[5]);                /* 温度低 8 位 */

    /* ---- 4. 换算（全是整数运算，不碰浮点，省 Flash） ----
     * 湿度  : RH% * 10 = hum_code * 1000 / 2^20
     * 温度  : T℃  * 10 = temp_code * 2000 / 2^20 - 500   (乘 200~×10 →  2000)  */
    *hX10 = (uint16_t)((hum_code * 1000u) >> 20);
    {
        int32_t t = (int32_t)((temp_code * 2000u) >> 20) - 500;
        if (t < 0) t = 0;                      /* 防御：负温度先按 0 处理（可扩展） */
        *tX10 = (uint16_t)t;
    }
    return AHT20_OK;
}