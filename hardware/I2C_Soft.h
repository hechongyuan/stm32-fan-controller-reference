#ifndef __I2C_SOFT_H
#define __I2C_SOFT_H

#include "stm32f10x.h"

/* 软件 I2C 总线的公共驱动（PB8=SCL / PB9=SDA）。
   整个工程只有一颗总线，OLED(0x3C) 和 AHT20(0x38) 挂在上面，地址不同互不干扰。
   所有上层驱动（OLED、AHT20）只调用这一组函数，不许自己写 GPIO 位操作。 */

void I2C_Soft_Init(void);   /* 初始化总线：SCL/SDA 开漏输出 */

/* 进阶三步：如果只发/收，直接用下面两个便捷函数即可 */

/* 向设备写 1 个寄存器/命令字节（reg=寄存器地址，value=数据）
   返回 0=收到 ACK，非 0=设备没应答（没接、地址错） */
uint8_t I2C_Soft_WriteReg(uint8_t devAddr, uint8_t reg, uint8_t value);

/* 向设备写一串数据（reg 之后依次发 buf[0..len-1]） */
uint8_t I2C_Soft_WriteBuf(uint8_t devAddr, uint8_t reg, const uint8_t *buf, uint8_t len);

/* 从设备连续读 len 个字节，存到 buf（先发寄存器地址再接回） */
uint8_t I2C_Soft_ReadRegs(uint8_t devAddr, uint8_t reg, uint8_t *buf, uint8_t len);

/* 从设备"无地址直读" len 个字节：起始→地址+读→逐个收→停止。
 * 给没有寄存器寻址的器件用（例如 AHT20：触发测量后直接读裸数据） */
uint8_t I2C_Soft_ReadRaw(uint8_t devAddr, uint8_t *buf, uint8_t len);

#endif