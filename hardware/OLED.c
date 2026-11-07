/* ============================================================================
 * OLED.c —— SSD1306 显示屏驱动（128x64，软件 I2C，地址 0x3C）
 * ----------------------------------------------------------------------------
 * "企业级"关键设计：显存缓冲（framebuffer）。
 *   - 所有 OLED_ShowXxx 只往内存里的 1KB 缓冲写像素，不碰屏幕；
 *   - 到 OLED_Update() 才把整块缓冲发到屏幕。
 * 好处：① 刷新不闪烁；② 屏幕更新和传感器读取不会交错（先全部画完再刷）；
 *       ③ 想"画面切割/局部刷新"时缓冲就是底稿。
 *
 * 软件 I2C 发送说明：每字节前面要带控制字节
 *   0x00 = 它后面的 1 字节是"命令"
 *   0x40 = 它后面的 N 字节是"显示数据"
 * ========================================================================== */
#include "OLED.h"
#include "I2C_Soft.h"

#define OLED_ADDR  0x3C        /* SSD1306 在 0x3C/0x3D（看模块背面的电阻） */

/* 显存缓冲：8 个显存页 × 128 列。g_ram[page][col]，1 位 = 1 像素 */
static uint8_t g_ram[OLED_PAGES][OLED_WIDTH];

/* ============================ 底层：命令/数据发送 ============================ */

/* 发一条命令（SSD1306 用"控制字节 0x00 + 命令字节"区分） */
static void OLED_SendCmd(uint8_t cmd)
{
    I2C_Soft_WriteReg(OLED_ADDR, 0x00, cmd);   /* reg=0x00 表示这是命令 */
}

/* 发一块显示数据（reg=0x40 表示后面全是数据） */
static void OLED_SendData(const uint8_t *buf, uint16_t len)
{
    I2C_Soft_WriteBuf(OLED_ADDR, 0x40, buf, (uint8_t)len);
}

/* ============================ 初始化 ============================ */

void OLED_Init(void)
{
    uint8_t i;
    I2C_Soft_Init();                     /* 总线也是 OLED 负责初始化的（AHT20 复用） */

    /* SSD1306 上电必须按顺序喂的命令序列（照抄厂商序列即可，逐行注释） */
    OLED_SendCmd(0xAE);   /* 显示关闭（进入配置阶段） */
    OLED_SendCmd(0xD5); OLED_SendCmd(0x80);   /* 时钟分频 */
    OLED_SendCmd(0xA8); OLED_SendCmd(0x3F);   /* 复用比 63（128x64） */
    OLED_SendCmd(0xD3); OLED_SendCmd(0x00);   /* 显示偏移 0 */
    OLED_SendCmd(0x40);                       /* 起始行 0 */
    OLED_SendCmd(0x8D); OLED_SendCmd(0x14);   /* 电荷泵开启（内部升压，必须） */
    OLED_SendCmd(0x20); OLED_SendCmd(0x00);   /* 寻址模式：水平 */
    OLED_SendCmd(0xA1);                       /* 段重映射（左右镜像修正） */
    OLED_SendCmd(0xC8);                       /* COM 扫描方向修正 */
    OLED_SendCmd(0xDA); OLED_SendCmd(0x12);   /* COM 引脚配置 */
    OLED_SendCmd(0x81); OLED_SendCmd(0xCF);   /* 对比度 */
    OLED_SendCmd(0xD9); OLED_SendCmd(0xF1);   /* 预充电周期 */
    OLED_SendCmd(0xDB); OLED_SendCmd(0x40);   /* VCOM 检测电平 */
    OLED_SendCmd(0xA4);                       /* 显示 RAM 内容 */
    OLED_SendCmd(0xA6);                       /* 正常显示（黑底亮字） */

    for (i = 0; i < OLED_PAGES; i++)
    {
        uint8_t j;
        for (j = 0; j < OLED_WIDTH; j++)
            g_ram[i][j] = 0;                 /* 缓冲清空 */
    }
    OLED_Update();                           /* 刷一次，屏幕上就是全黑 */

    OLED_SendCmd(0xAF);                      /* 显示打开 */
}

/* ============================ 显存绘制 ============================ */

void OLED_Clear(void)
{
    uint8_t i, j;
    for (i = 0; i < OLED_PAGES; i++)
        for (j = 0; j < OLED_WIDTH; j++)
            g_ram[i][j] = 0;
}

/* ============================ 精简 ASCII 字体 ============================ */

/* 8x8 点阵子集：大写字母、数字、常用符号。竖排每字节 8 行，bit7 在最左。
 * 顺序必须与 FONT_CHARS 字符串一一对应。想换全字库就用你 4-1 的 OLED_Font.h。 */
static const char FONT_CHARS[] =
    " 0123456789:.-%/ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const uint8_t FONT_8x8[][8] =
{
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x66,0x3C}, /* '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E}, /* '1' */
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E}, /* '2' */
    {0x3C,0x66,0x06,0x0C,0x06,0x06,0x66,0x3C}, /* '3' */
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C}, /* '4' */
    {0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C}, /* '5' */
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x66,0x3C}, /* '6' */
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30}, /* '7' */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C}, /* '8' */
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x66,0x3C}, /* '9' */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, /* ':' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* '.' */
    {0x00,0x63,0x66,0x0C,0x18,0x30,0x66,0x63}, /* '%' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* '/' */
    {0x00,0x3C,0x66,0x66,0x7E,0x66,0x66,0x66}, /* 'A' */
    {0x00,0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C}, /* 'B' */
    {0x00,0x3C,0x66,0x60,0x60,0x60,0x66,0x3C}, /* 'C' */
    {0x00,0x78,0x6C,0x66,0x66,0x66,0x6C,0x78}, /* 'D' */
    {0x00,0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E}, /* 'E' */
    {0x00,0x7E,0x60,0x60,0x7C,0x60,0x60,0x60}, /* 'F' */
    {0x00,0x3E,0x60,0x60,0x6E,0x66,0x66,0x3E}, /* 'G' */
    {0x00,0x66,0x66,0x66,0x7E,0x66,0x66,0x66}, /* 'H' */
    {0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E}, /* 'I' */
    {0x00,0x1E,0x0C,0x0C,0x0C,0x6C,0x6C,0x38}, /* 'J' */
    {0x00,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66}, /* 'K' */
    {0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x7E}, /* 'L' */
    {0x00,0x63,0x77,0x7F,0x6B,0x63,0x63,0x63}, /* 'M' */
    {0x00,0x62,0x72,0x7A,0x6E,0x66,0x66,0x66}, /* 'N' */
    {0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x3C}, /* 'O' */
    {0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,0x60}, /* 'P' */
    {0x00,0x3C,0x66,0x66,0x66,0x6E,0x3C,0x06}, /* 'Q' */
    {0x00,0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66}, /* 'R' */
    {0x00,0x3E,0x60,0x60,0x3C,0x06,0x06,0x7C}, /* 'S' */
    {0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18}, /* 'T' */
    {0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3C}, /* 'U' */
    {0x00,0x66,0x66,0x66,0x66,0x3C,0x3C,0x18}, /* 'V' */
    {0x00,0x63,0x63,0x6B,0x7F,0x77,0x63,0x63}, /* 'W' */
    {0x00,0x66,0x66,0x3C,0x3C,0x66,0x66,0x66}, /* 'X' */
    {0x00,0x66,0x66,0x3C,0x18,0x18,0x18,0x18}, /* 'Y' */
    {0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* 'Z' */
};

/* 在字体表里找字符位置，找不到返回 0（画空格） */
static uint8_t OLED_FontIndex(char ch)
{
    uint8_t i = 0;
    while (FONT_CHARS[i] != '\0')
    {
        if (FONT_CHARS[i] == ch)
            return i;
        i++;
    }
    return 0;   /* 不在表里（小写、汉字等）→ 显示空格 */
}

/* 在显存指定字符位置画一个字符 */
void OLED_ShowChar(uint8_t line, uint8_t col, char ch)
{
    uint8_t row, idx;

    if (line >= OLED_LINES || col >= (OLED_WIDTH / OLED_CHAR_W))
        return;                             /* 越界就丢弃 (防御) */

    idx = OLED_FontIndex(ch);
    for (row = 0; row < OLED_CHAR_H; row++) /* 8 行，写完一整个字符 */
    {
        uint8_t page = line * OLED_CHAR_H;  /* 字符行 → 像素行 */
        uint8_t p    = page / 8;            /* 每 8 行一个显存页 */
        uint8_t bit  = page % 8;

        /* 完整 8x8 字符都落在同一个显存页里：把字模直接写进缓冲对应位置 */
        if (p < OLED_PAGES)
        {
            uint8_t px = col * OLED_CHAR_W; /* 字符列 → 像素列 */
            g_ram[p][px + (row - bit)] = FONT_8x8[idx][row];   /* row 里的高行位对齐 */
        }
    }
}

/* 画字符串：一个字符一个字符地写（读到 '\0' 停） */
void OLED_ShowStr(uint8_t line, uint8_t col, const char *str)
{
    while (*str != '\0')
    {
        OLED_ShowChar(line, col++, *str);
        str++;
    }
}

/* 画整数：len 固定位数，位数不够高位补空格
 *（保证"DUTY:  5%"和"DUTY: 80%"对齐，界面才不乱跳） */
void OLED_ShowNum(uint8_t line, uint8_t col, uint32_t value, uint8_t len)
{
    uint8_t i;
    char buf[10];
    for (i = 0; i < len; i++)
    {
        buf[len - 1 - i] = (char)('0' + (value % 10));  /* 从个位往前填 */
        value /= 10;
    }
    for (i = 0; i < len; i++)
        OLED_ShowChar(line, col + i, buf[i]);
}

/* 画"整体×10"的小数：x10=125 → 输出 12.5 */
void OLED_ShowFloat1(uint8_t line, uint8_t col, uint16_t x10)
{
    uint16_t inte = x10 / 10;          /* 整数部分 */
    uint16_t frac = x10 % 10;          /* 小数 1 位 */
    OLED_ShowNum(line, col,   inte, 2);   /* 整数 2 位 */
    OLED_ShowChar(line, col + 2, '.');
    OLED_ShowChar(line, col + 3, (char)('0' + frac));
}

/* ============================ 刷屏 ============================ */

void OLED_Update(void)
{
    uint8_t page;

    /* 先设列地址 0~127，再设页地址 0~7，水平模式下一口气把 8 页数据发完 */
    OLED_SendCmd(0x21); OLED_SendCmd(0x00); OLED_SendCmd(0x7F);
    OLED_SendCmd(0x22); OLED_SendCmd(0x00); OLED_SendCmd(0x07);

    for (page = 0; page < OLED_PAGES; page++)
        OLED_SendData(g_ram[page], OLED_WIDTH);   /* 每页 128 字节 */
}