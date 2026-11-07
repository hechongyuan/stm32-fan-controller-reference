/* ============================================================================
 * I2C_Soft.c —— 软件 I2C 总线驱动（GPIO 模拟 I2C 时序）
 * ----------------------------------------------------------------------------
 * 为什么用"软件 I2C"而不是芯片自带的硬件 I2C？
 *   1) 软件版随便哪两个脚都能当 SCL/SDA，接线灵活（本项目 PB8/PB9）；
 *   2) 硬件 I2C 的时序坑多（锁死、时钟拉伸），新手极易卡住；
 *   3) OLED/AHT20 数据量小，软件模拟的速度完全够。
 *
 * 时序原理（四个基本动作，背下来面试能画图）：
 *   起始 START : SCL=1 时 SDA 由 1→0（电平跳变，吸引从机注意）
 *   停止 STOP  : SCL=1 时 SDA 由 0→1
 *   送 1 位    : SCL=0 时摆 SDA 电平 → SCL=1 让从机采样 → SCL=0
 *   收 1 位    : SCL=0 时释放 SDA(高阻) → SCL=1 读回电平 → SCL=0
 *
 * 工程注意：同一时刻全工程只有一个"传输动作"，OLED 与 AHT20 都在主循环
 * 串行执行，天然不会撞车；但绝不能在 OLED 正在刷屏时插入 AHT20 读取
 * （所以 app 层把读取安排到刷屏之外，见 app_display/app_sensor 注释）。
 * ========================================================================== */
#include "I2C_Soft.h"

/* ---- 常量：引脚定义（全部改这里即可换脚） ---- */
#define I2C_SCL_PORT   GPIOB
#define I2C_SCL_PIN    GPIO_Pin_8
#define I2C_SDA_PORT   GPIOB
#define I2C_SDA_PIN    GPIO_Pin_9

/* ---- 四个位操作：开漏输出时，SDA 读取读的是引脚实际电平 ---- */
static void SCL_H(void) { I2C_SCL_PORT->BSRR = I2C_SCL_PIN; }  /* 拉高 SCL */
static void SCL_L(void) { I2C_SCL_PORT->BRR  = I2C_SCL_PIN; }  /* 拉低 SCL */
static void SDA_H(void) { I2C_SDA_PORT->BSRR = I2C_SDA_PIN; }  /* 拉高 SDA */
static void SDA_L(void) { I2C_SDA_PORT->BRR  = I2C_SDA_PIN; }  /* 拉低 SDA */

/* 读 SDA 电平：返回 1=高 0=低（开漏输出 + 外部上拉，读输入照样有效） */
static uint8_t SDA_R(void)
{
    return (uint8_t)((I2C_SDA_PORT->IDR & I2C_SDA_PIN) ? 1 : 0);
}

/* 一个时钟周期的小停顿：约几百 ns，软件 I2C 无需严格，够慢即稳 */
static void I2C_Delay(void)
{
    volatile uint8_t i;
    for (i = 0; i < 4; i++) { ; }
}

void I2C_Soft_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 开漏输出：既能输出电平，又能靠外部上拉被从机拉低（SDA 应答位就这么来） */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = I2C_SCL_PIN | I2C_SDA_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SDA_H();     /* 空闲态：两条线都高（I2C 规范要求） */
    SCL_H();
}

/* 起始条件：SCL 高电平期间 SDA 由 1 变 0 */
static void I2C_Start(void)
{
    SDA_H(); SCL_H(); I2C_Delay();   /* 先都拉到空闲 */
    SDA_L(); I2C_Delay();            /* SCL 还高着，SDA 下拉 → 起始 */
    SCL_L(); I2C_Delay();            /* 拉低 SCL，准备传数据 */
}

/* 停止条件：SCL 高电平期间 SDA 由 0 变 1 */
static void I2C_Stop(void)
{
    SCL_L(); SDA_L(); I2C_Delay();
    SCL_H(); I2C_Delay();            /* SCL 先上去 */
    SDA_H(); I2C_Delay();            /* SDA 再上去 → 停止 */
}

/* 发送 1 个字节，返回从机应答：0=有 ACK(设备在)，1=NAK(没设备) */
static uint8_t I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80) SDA_H(); else SDA_L();   /* 高位先发 */
        byte <<= 1;
        I2C_Delay();
        SCL_H(); I2C_Delay();       /* SCL 上升沿，从机采这一位 */
        SCL_L(); I2C_Delay();
    }
    /* 第 9 个时钟：读 ACK 位（从机把 SDA 拉低=应答） */
    SDA_H();                        /* 释放 SDA，让从机控制 */
    I2C_Delay();
    SCL_H(); I2C_Delay();
    uint8_t ack = SDA_R();          /* 读回：0=有应答 */
    SCL_L(); I2C_Delay();
    return ack;
}

/* 读取 1 个字节，ack=1 表示读完还要继续收(回复 ACK)，ack=0 表示这是最后一个 */
static uint8_t I2C_RecvByte(uint8_t ack)
{
    uint8_t i, data = 0;
    for (i = 0; i < 8; i++)
    {
        SCL_H(); I2C_Delay();       /* SCL 高，数据已由从机摆在 SDA 上 */
        data = (uint8_t)((data << 1) | SDA_R());   /* 读入一位 */
        SCL_L(); I2C_Delay();
    }
    /* 发应答：要接着收就 SDA=0(ACK)，不收就 SDA=1(NAK) 让从机停 */
    if (ack) SDA_L(); else SDA_H();
    I2C_Delay();
    SCL_H(); I2C_Delay();
    SCL_L(); I2C_Delay();
    return data;
}

/* 便捷封装 1：写 1 个寄存器
 * 流程：起始 → 设备地址+写位 → 寄存器地址 → 数据 → 停止
 * 返回 0=全程成功，非 0=设备无应答 */
uint8_t I2C_Soft_WriteReg(uint8_t devAddr, uint8_t reg, uint8_t value)
{
    uint8_t err = 0;
    I2C_Start();
    err |= I2C_SendByte((uint8_t)(devAddr << 1));   /* 地址左移 1 位 = 写 */
    err |= I2C_SendByte(reg);
    err |= I2C_SendByte(value);
    I2C_Stop();
    return err;
}

/* 便捷封装 2：写寄存器 + 一串数据（AHT20 的 0xAC 测量命令就这么发） */
uint8_t I2C_Soft_WriteBuf(uint8_t devAddr, uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t err = 0, i;
    I2C_Start();
    err |= I2C_SendByte((uint8_t)(devAddr << 1));   /* 设备地址 + 写位 */
    err |= I2C_SendByte(reg);                       /* 寄存器/命令地址 */
    for (i = 0; i < len; i++)
        err |= I2C_SendByte(buf[i]);                /* 依次发数据 */
    I2C_Stop();
    return err;
}

/* 便捷封装 3：写寄存器后再连续读
 * 流程：起始→地址+写→reg→(重起始)→地址+读→逐字节收回→停止 */
uint8_t I2C_Soft_ReadRegs(uint8_t devAddr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t err = 0, i;
    I2C_Start();
    err |= I2C_SendByte((uint8_t)(devAddr << 1));   /* 先写寄存器指针 */
    err |= I2C_SendByte(reg);
    I2C_Start();                                    /* 重新起始 */
    err |= I2C_SendByte((uint8_t)((devAddr << 1) | 0x01));  /* 地址 + 读位 */
    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_RecvByte(((uint8_t)(i < len - 1)));   /* 最后一个回 NAK */
    }
    I2C_Stop();
    return err;
}

/* 便捷封装 4：无寄存器直读（AHT20 这类没有"寄存器地址"概念的器件）
 * 流程：起始 → 地址+读位 → 逐字节收 → 停止 */
uint8_t I2C_Soft_ReadRaw(uint8_t devAddr, uint8_t *buf, uint8_t len)
{
    uint8_t err = 0, i;
    I2C_Start();
    err |= I2C_SendByte((uint8_t)((devAddr << 1) | 0x01));  /* 地址 + 读位 */
    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_RecvByte(((uint8_t)(i < len - 1)));   /* 最后一个回 NAK */
    }
    I2C_Stop();
    return err;
}