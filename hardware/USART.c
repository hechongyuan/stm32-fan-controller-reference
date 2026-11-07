/* ============================================================================
 * USART.c —— 串口驱动（USART1：PA9=TX / PA10=RX，115200 8N1）
 * ----------------------------------------------------------------------------
 * 发送：轮询"发送移位寄存器空"(TC) 标志，慢慢推字节，简单可靠。
 * 接收：企业级做法——中断把收到的每个字节塞进一个环形缓冲(Ring Buffer)，
 *      主循环空闲时再取走处理。好处：
 *       ① 字节来的时间不定，中断收缓冲不会漏；
 *       ② 主循环在"方便的时候"再解析，不被打断时序。
 * 环形缓冲：头尾指针环绕，可用长度 = (head - tail)。
 * ========================================================================== */
#include "USART.h"
#include <stdio.h>

#define RX_BUF_SIZE   64   /* 接收环形缓冲容量 */

static volatile uint16_t g_rx_head = 0;   /* 写指针：中断里移动 */
static volatile uint16_t g_rx_tail = 0;   /* 读指针：主循环移动 */
static volatile uint8_t  g_rx_buf[RX_BUF_SIZE];

void USART_Init(void)
{
    GPIO_InitTypeDef       GPIO_InitStructure;
    USART_InitTypeDef      USART_InitStructure;
    NVIC_InitTypeDef       NVIC_InitStructure;

    /* ---- 1. 时钟：USART1 和它的 GPIO 都在 APB2 ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* ---- 2. 引脚：TX 复用推挽输出，RX 浮空输入 ---- */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;      /* PA9 = TX1 */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;     /* PA10 = RX1 */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ---- 3. 串口参数：115200、8 数据位、1 停止位、无校验 ---- */
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    /* ---- 4. 打开接收中断(RXNE)：收到 1 字节就进 ISR ---- */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    /* ---- 5. NVIC 开串口中断通道 ---- */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;   /* 比按键高 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);      /* 最后启动串口 */
}

/* ============================ 发送 ============================ */

void USART_SendByte(uint8_t byte)
{
    /* 等上一字节真正送完(TC=发送完成)，再写下一字节，防止写冲突 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    USART_SendData(USART1, byte);
}

void USART_SendStr(const char *str)
{
    while (*str != '\0')
        USART_SendByte((uint8_t)*str++);
}

/* 二进制帧发送：逐字节发（协议层组好帧后调用） */
void USART_SendBuf(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
        USART_SendByte(buf[i]);
}

/* printf 重定向：Keil 里 printf 最终调到这个函数 */
int fputc(int ch, FILE *f)
{
    (void)f;
    USART_SendByte((uint8_t)ch);
    return ch;
}

/* ============================ 接收（环形缓冲） ============================ */

/* 中断回调：收进缓冲（在 stm32f10x_it.c 的 USART1_IRQHandler 里调用） */
void USART_RxISR(void)
{
    /* 先把标志位读进来再清：读 DR 会同时清 RXNE */
    uint8_t byte = USART_ReceiveData(USART1);

    uint16_t next = (uint16_t)((g_rx_head + 1) % RX_BUF_SIZE);  /* 下一写位置 */
    if (next != g_rx_tail)          /* 还空着 = 没被主循环读光，才存（防覆盖） */
    {
        g_rx_buf[g_rx_head] = byte;
        g_rx_head = next;
    }
    /* 如果写满(追上 tail)就丢弃最老的逻辑：这里简单丢弃新字节即可 */
}

uint16_t USART_RxAvailable(void)
{
    return (uint16_t)((g_rx_head + RX_BUF_SIZE - g_rx_tail) % RX_BUF_SIZE);
}

uint8_t USART_RxReadByte(void)
{
    uint8_t byte;
    if (USART_RxAvailable() == 0)
        return 0;                   /* 没数据就返回 0（协议层先查可得再读） */
    byte = g_rx_buf[g_rx_tail];
    g_rx_tail = (uint16_t)((g_rx_tail + 1) % RX_BUF_SIZE);
    return byte;
}