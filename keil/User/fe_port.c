// fe_port.c — FasterEdge MCU 平台移植层参考实现（C51/8051 版）
// 本文件为移植模板：把所有 TODO 处替换为具体 8051 芯片的实现即可。
// 文件末尾附 STC89/STC15（IAP/EEPROM）与 AT89S52 的参考片段。
#include "fe_port.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// ============================================================
// 格式化输出
// ============================================================
// 默认委托标准 vsnprintf；Keil C51 无 snprintf 时，可改用
// vsprintf + 手动截断，或将 putchar 重定向到串口后用 printf。
int fe_snprintf(char *buf, u16 size, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    if (n < 0) { buf[0] = 0; return 0; }
    if ((u16)n >= size) buf[size - 1] = 0;
    return n;
}

// ============================================================
// 串口（UART）
// ============================================================
static fe_port_uart_rx_cb_t g_rx_cb = NULL;
static void *g_rx_user = NULL;

void fe_port_uart_init(u8 port, u32 baud, fe_port_uart_rx_cb_t rx_cb, void *user) {
    g_rx_cb = rx_cb;
    g_rx_user = user;
    // TODO: 初始化 UART(port, baud)：
    //   SCON = 0x50; TMOD |= 0x20; TH1 = 256 - FOSC/(12*32*baud);
    //   TR1 = 1; ES = 1; EA = 1;
    //   RX 中断里调用 if (g_rx_cb) g_rx_cb(SBUF, g_rx_user);
    (void)port; (void)baud;
}

u16 fe_port_uart_write(u8 port, const u8 *data, u16 len) {
    u16 i;
    for (i = 0; i < len; i++) {
        // TODO: while (!TI); TI = 0; SBUF = data[i];
    }
    (void)port;
    return len;
}

u8 fe_port_uart_available(u8 port) {
    (void)port;
    // TODO: return RI ? 1 : 0;
    return FALSE;
}

int fe_port_uart_read(u8 port) {
    (void)port;
    // TODO: if (!RI) return -1; RI = 0; return SBUF;
    return -1;
}

void fe_port_uart_close(u8 port) {
    (void)port;
    // TODO: ES = 0;
}

// ============================================================
// EEPROM（存储配置/密钥）
// ============================================================
u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen) {
    // TODO: 从 EEPROM(addr) 读字符串
    (void)addr; (void)out; (void)outlen;
    if (outlen) out[0] = 0;
    return FALSE;
}

u8 fe_port_eeprom_set_str(u16 addr, const char *value) {
    // TODO: 写字符串到 EEPROM(addr)
    (void)addr; (void)value;
    return TRUE;
}

u8 fe_port_eeprom_get_u32(u16 addr, u32 *out) {
    // TODO: 读 4 字节（小端）到 *out
    (void)addr; (void)out;
    return FALSE;
}

u8 fe_port_eeprom_set_u32(u16 addr, u32 value) {
    // TODO: 写 4 字节（小端）
    (void)addr; (void)value;
    return TRUE;
}

// ============================================================
// 系统时间
// ============================================================
u32 fe_port_time_now(void) {
    // TODO: 定时器 0 秒中断计数返回 epoch 秒
    return 0;
}

void fe_port_time_set(u32 epoch) {
    // TODO: 设置计数基准
    (void)epoch;
}

// ============================================================
// 随机数
// ============================================================
void fe_port_random_fill(u8 *buf, u16 len) {
    u16 i;
    // TODO: 熵源建议：未初始化 RAM、ADC 噪声、定时器低字节
    for (i = 0; i < len; i++) buf[i] = (u8)(i * 31 + 7);
}

// ============================================================
// 延时
// ============================================================
void fe_port_delay_ms(u32 ms) {
    // TODO: 软件延时（FOSC 12MHz 时约 1000 次循环/ms）
    volatile u32 i;
    for (; ms > 0; ms--)
        for (i = 0; i < 1000; i++) ;
}

/*
 * ============================================================
 * STC89/STC15（内部 EEPROM/IAP）参考实现片段
 * ============================================================
 *   #include "stc15.h"   // 或 stc89c52rc.h
 *
 *   void iap_idle(void)      { IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0; IAP_ADDRH = 0; IAP_ADDRL = 0; }
 *   u8    iap_read(u16 addr) { u8 d; IAP_CONTR = 0x80; IAP_CMD = 1; IAP_ADDRL = addr; IAP_ADDRH = addr>>8;
 *                              IAP_TRIG = 0x5A; IAP_TRIG = 0xA5; d = IAP_DATA; iap_idle(); return d; }
 *   void iap_write(u16 addr, u8 d) { IAP_CONTR = 0x80; IAP_CMD = 2; IAP_ADDRL = addr; IAP_ADDRH = addr>>8;
 *                              IAP_DATA = d; IAP_TRIG = 0x5A; IAP_TRIG = 0xA5; iap_idle(); }
 *   void iap_erase(u16 addr) { IAP_CONTR = 0x80; IAP_CMD = 3; IAP_ADDRL = addr; IAP_ADDRH = addr>>8;
 *                              IAP_TRIG = 0x5A; IAP_TRIG = 0xA5; iap_idle(); }
 *
 *   // EEPROM 写字符串（按扇区，先擦后写；扇区 512B）
 *   u8 fe_port_eeprom_set_str(u16 addr, const char *v) {
 *       u16 i, base = addr & 0xFE00;
 *       u8  page[512];                    // xdata
 *       for (i = 0; i < 512; i++) page[i] = iap_read(base + i);
 *       for (i = 0; v[i] && (addr + i) < base + 512; i++) page[addr - base + i] = v[i];
 *       page[addr - base + i] = 0;
 *       iap_erase(base);
 *       for (i = 0; i < 512; i++) iap_write(base + i, page[i]);
 *       return TRUE;
 *   }
 *
 * ============================================================
 * AT89S52（无内置 EEPROM，需外接 24C02 I2C）参考片段
 * ============================================================
 *   // 用 I2C 时序读写 24C02（512B，地址 0xA0）：
 *   //   i2c_start(); i2c_write(0xA0); i2c_write(addr); i2c_start(); i2c_write(0xA1);
 *   //   *out = i2c_read(0); i2c_stop();
 *   // 写：i2c_start(); i2c_write(0xA0); i2c_write(addr);
 *   //     i2c_write(byte); i2c_stop(); 延时 5ms 等内部写周期
 *
 * ============================================================
 */