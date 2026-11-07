#ifndef __AHT20_H
#define __AHT20_H

/* 初始化传感器：发送校准命令。返回 0=正常，非 0=应答失败（没接/地址错） */
uint8_t AHT20_Init(void);

/* 读一次温湿度（阻塞约 80ms 等测量完成）。
 * 成功返回 0，并把结果写成"整数×10"：tX10=255 → 25.5℃，hX10=600 → 60.0%
 * 失败返回非 0，输出值不动。 */
uint8_t AHT20_Read(uint16_t *tX10, uint16_t *hX10);

#endif