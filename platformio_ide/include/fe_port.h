// FasterEdge 开源项目 - Github: https://github.com/FasterEdge - Gitee: https://gitee.com/FasterEdge
// fe_port.h — FasterEdge MCU 平台移植层（C51/8051 版）
// 平台相关能力在此抽象：UART 收发、EEPROM 存储（模拟 NVS）、
// 系统时间（定时器）、随机数。C51 无网络，不提供 WiFi/TCP。
// 移植到具体 8051（STC/AT89/新唐等）时只需实现本文件。
#ifndef FE_PORT_H
#define FE_PORT_H

#include "fe.h"   // u8/u16/u32 类型、TRUE/FALSE

// ============================================================
// 格式化输出（C51 标准库无 snprintf，统一走本函数）
// ============================================================
// 与 snprintf 语义一致：至多 size 字节、始终 NUL 结尾。
// 实现见 fe_port.c（默认委托 vsnprintf；C51 下可替换为
// sprintf + 长度截断，或使用 C51 的 printf 重定向）。
int fe_snprintf(char *buf, u16 size, const char *fmt, ...);

// ============================================================
// 串口（UART）
// ============================================================
typedef void (*fe_port_uart_rx_cb_t)(u8 byte, void *user);

// 初始化串口：port 编号（0=UART0/默认），baud 波特率，rx 回调（可为 NULL）
void fe_port_uart_init(u8 port, u32 baud, fe_port_uart_rx_cb_t rx_cb, void *user);
// 发送 len 字节，返回实际发送字节数
u16 fe_port_uart_write(u8 port, const u8 *data, u16 len);
// 是否有数据可读
u8  fe_port_uart_available(u8 port);
// 读一个字节（无数据返回 -1）
int fe_port_uart_read(u8 port);
// 关闭串口
void fe_port_uart_close(u8 port);

// ============================================================
// EEPROM（存储配置/密钥；C51 常见方案：STC 内置 IAP、
// 24C02 I2C、或片外 Flash 模拟）
// ============================================================
// 读字符串：addr 起始地址，out 输出缓冲，outlen 缓冲长度。成功返回 TRUE
u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen);
// 写字符串：addr 起始地址。成功返回 TRUE
u8 fe_port_eeprom_set_str(u16 addr, const char *value);
// 读 u32：成功返回 TRUE
u8 fe_port_eeprom_get_u32(u16 addr, u32 *out);
// 写 u32：成功返回 TRUE
u8 fe_port_eeprom_set_u32(u16 addr, u32 value);

// ============================================================
// 系统时间（epoch 秒，u32 到 2106 年够用）
// ============================================================
u32 fe_port_time_now(void);
void fe_port_time_set(u32 epoch);

// ============================================================
// 随机数
// ============================================================
void fe_port_random_fill(u8 *buf, u16 len);

// ============================================================
// 寄存器 / 存储空间读写——RegAbility 需要
// ============================================================
// 读特殊功能寄存器（SFR，地址 0x80-0xFF）。失败返回 0
u8 fe_port_sfr_read(u8 addr);
// 写特殊功能寄存器（SFR）。用户负责保证地址合法
void fe_port_sfr_write(u8 addr, u8 val);
// 读外部扩展 RAM（xdata，16 位地址）
u8 fe_port_xram_read(u16 addr);
// 写外部扩展 RAM（xdata）
void fe_port_xram_write(u16 addr, u8 val);

// ============================================================
// 芯片信息——ChipData 需要
// ============================================================
// 生成芯片信息 JSON 到 out
void fe_port_chip_info(char *out, u16 outlen);

// ============================================================
// 延时 / 喂狗（毫秒）
// ============================================================
void fe_port_delay_ms(u32 ms);

#endif // FE_PORT_H
