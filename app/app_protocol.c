/* ============================================================================
 * app_protocol.c —— 自定义串口协议 + CRC8
 * ----------------------------------------------------------------------------
 * 帧格式：0xAA 0x55 | len | data | CRC8
 *   CRC8 覆盖"len + 全部数据"，用查表法（提前把 256 种收入免逐位算）。
 * 消息方向：
 *   上行(本机→上位机)  ：每 1 秒一帧，上报状态/档位/温湿度；
 *   下行(上位机→本机)  ：主循环解析环形缓冲，支持"设档/开关"两条命令。
 * 这样面试时你能说：我做了"自定义协议 + 校验 + 双向通道"，可闭环演示。
 * ========================================================================== */
#include "app_protocol.h"
#include "USART.h"
#include "app_fan.h"
#include "app_sensor.h"
#include <string.h>

/* ---- 帧常量 ---- */
#define FRAME_HDR1     0xAA
#define FRAME_HDR2     0x55

/* 上报帧数据段布局（与 .h 中注释对应的偏移量） */
#define RX_DATA_STATE  0     /* 运行状态 */
#define RX_DATA_LEVEL  1     /* 档位 */
#define TX_DATA_LEN    6     /* 上报帧数据段长度 */

/* ---- CRC8 查表 --------------------------------
 * 多项式 0x31（即 CRC-8/MAXIM，MSB-first）。
 * Init 时一次性生成 256 项查表（企业做法通常是编译期常数表，
 * 这里用运行时生成，代码短、少女少错，效果等价）。
 * 查表法：新字节先和旧 CRC 异或，再查表，不用你挨位算。 */
static uint8_t g_crc8_table[256];
static uint8_t g_crc_inited = 0;

static void CRC8_Init(void)
{
    uint16_t i, j;
    for (i = 0; i < 256; i++)
    {
        uint8_t crc = (uint8_t)i;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);   /* 生成多项式 */
            else
                crc = (uint8_t)(crc << 1);
        }
        g_crc8_table[i] = crc;
    }
    g_crc_inited = 1;
}

static uint8_t CRC8_Calc(const uint8_t *buf, uint16_t len)
{
    uint8_t crc = 0;
    uint16_t i;
    for (i = 0; i < len; i++)
        crc = g_crc8_table[crc ^ buf[i]];      /* 查表更新（比逐位快 8 倍） */
    return crc;
}

/* ============================ 初始化 ============================ */

void AppProtocol_Init(void)
{
    if (!g_crc_inited)
        CRC8_Init();                          /* 建查表只跑一次 */
}

/* ============================ 上行：每秒一帧 ============================ */

void AppProtocol_Tick(void)
{
    uint8_t frame[3 + TX_DATA_LEN + 1];       /* 0xAA 0x55 len data... crc */
    uint8_t dlen = TX_DATA_LEN;
    uint16_t temp = AppSensor_GetTempX10();
    uint16_t humi = AppSensor_GetHumX10();

    /* 组头 */
    frame[0] = FRAME_HDR1;
    frame[1] = FRAME_HDR2;
    frame[2] = dlen;

    /* 组数据段 */
    frame[3 + 0] = (uint8_t)(AppFan_IsRunning() ? 1 : 0);
    frame[3 + 1] = AppFan_GetLevel();
    frame[3 + 2] = (uint8_t)(temp >> 8);      /* 温度 ×10 高字节在前 */
    frame[3 + 3] = (uint8_t)(temp & 0xFF);
    frame[3 + 4] = (uint8_t)(humi >> 8);      /* 湿度 */
    frame[3 + 5] = (uint8_t)(humi & 0xFF);

    /* 帧尾：CRC 覆盖 [len..数据末尾]，不含帧头(固定字节，无需验) */
    frame[3 + dlen] = CRC8_Calc(&frame[2], 1 + dlen);
    (void)temp; (void)humi;

    USART_SendBuf(frame, sizeof(frame));
}

/* ============================ 下行：解析控制帧 ============================ */

void AppProtocol_Process(void)
{
    /* 状态小机：找帧头 → 收 len → 收数据 → 收 crc → 校验通过才执行。
     * 简单实现：每次处理一个完整帧（上位机 1s 一帧，频率低，可放宽）。 */
    static uint8_t st = 0;              /* 0=等帧头1 1=等帧头2 2=等len */
    static uint8_t len = 0, got = 0;
    static uint8_t buf[32];
    uint8_t b;

    while (USART_RxAvailable())         /* 把缓冲里的字节逐个"喂"给状态机 */
    {
        b = USART_RxReadByte();

        switch (st)
        {
            case 0: if (b == FRAME_HDR1)      st = 1;             break;
            case 1: if (b == FRAME_HDR2)      st = 2; else st = 0; break;
            case 2:
                if (b > 0 && b <= 30) { len = b; buf[0] = b;                    got = 0; st = 3; }
                else st = 0;                                              /* 长度非法，丢弃重来 */
                break;
            case 3:
                buf[1 + got] = b;                                   /* got 自数据处理段 */
                got++;
                if (got >= len) st = 4;
                break;
            case 4:;                                                /* b==CRC，校验 */
                if (b == CRC8_Calc(buf, 1 + len))                   /* 范围同上报 */
                {
                    switch (buf[1])                                 /* 命令码，参数从 buf[2] 起 */
                    {
                        case 0x01:                                  /* 设置档位 */
                            if (buf[2] <= 100)
                            {
                                while (AppFan_GetLevel() < buf[2]) { AppFan_LevelUp(); }
                            }
                            break;
                        case 0x02:                                  /* 开/关 */
                            if (buf[2] == 0) AppFan_Off();
                            else            AppFan_On();
                            break;
                        default: break;
                    }
                }
                st = 0;
                break;
            default: st = 0; break;
        }
        (void)b;
    }
}