// fe_port.c — FasterEdge MCU 平台移植层（C51/8051 版，SDCC 工具链）
// 用于 platformio_ide 工程（VS Code + PlatformIO 插件，STC 平台 + SDCC）：
//   platform = stc
// 8051 特殊功能寄存器直接操作（SDCC <8051.h> / <reg51.h>）。
// 默认晶振 FOSC=12000000UL（可在 platformio.ini build_flags 覆盖）。
#include "fe_port.h"

#include <8051.h>          // SDCC 标准 8051 SFR 定义（P0/TCON/TMOD/SCON/SBUF 等）
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef FOSC
#define FOSC 12000000UL
#endif

// ============================================================
// 格式化输出
// ============================================================
// 委托标准 vsprintf（SDCC 默认链接 printf 支持 %d/%u/%x/%s；
// 需要 %lu 时在 platformio.ini 加 -Dprintf=printf_large）。
int fe_snprintf(char *buf, u16 size, const char *fmt, ...) {
    va_list ap;
    char tmp[128];
    size_t n;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);      // 先格式化到临时缓冲
    va_end(ap);
    n = strlen(tmp);
    if (n >= size) n = size - 1;
    memcpy(buf, tmp, n);
    buf[n] = 0;
    return (int)n;
}

// ============================================================
// 串口（UART0）
// ============================================================
static fe_port_uart_rx_cb_t g_rx_cb = NULL;
static void *g_rx_user = NULL;

// 波特率定时器重载值：TH1 = 256 - FOSC/(12*32*baud)
static u8 baud_reload(u32 baud) {
    u32 t = FOSC / 12UL / 32UL;
    u8 v = 0;
    if (baud > 0) {
        t /= baud;
        if (t < 256) v = (u8)(256 - t);
        else v = 1;   // 波特率过低，取近似
    }
    return v;
}

void fe_port_uart_init(u8 port, u32 baud, fe_port_uart_rx_cb_t rx_cb, void *user) {
    (void)port;
    u8 reload = baud_reload(baud);
    TMOD = (TMOD & 0x0F) | 0x20;   // 定时器1 模式2（8 位自动重载）
    TH1 = reload;
    TL1 = reload;
    SCON = 0x50;                   // 模式1，REN=1 允许接收
    TR1 = 1;                       // 启动定时器1
    g_rx_cb = rx_cb;
    g_rx_user = user;
}

u16 fe_port_uart_write(u8 port, const u8 *data, u16 len) {
    u16 i;
    (void)port;
    for (i = 0; i < len; i++) {
        SBUF = data[i];
        while (!TI);               // 等待发送完成
        TI = 0;
    }
    return len;
}

u8 fe_port_uart_available(u8 port) {
    (void)port;
    return (SCON & 0x01) ? TRUE : FALSE;   // RI 置位
}

int fe_port_uart_read(u8 port) {
    u8 b;
    (void)port;
    if (!(SCON & 0x01)) return -1;         // 无数据
    b = SBUF;
    RI = 0;                                // 清 RI
    if (g_rx_cb) g_rx_cb(b, g_rx_user);
    return (int)b;
}

void fe_port_uart_close(u8 port) {
    (void)port;
    ES = 0;
}

// ============================================================
// EEPROM（存储配置/密钥）
// ============================================================
// STC 系列 8051 内置 EEPROM（IAP）。SDCC 访问 IAP 寄存器：
//   IAP_CONTR/ IAP_CMD/ IAP_TRIG/ IAP_ADDRH/ IAP_ADDRL/ IAP_DATA
// 参考（STC15）：
//   void iap_operate(u8 cmd, u16 addr, u8 *data) {
//       IAP_CONTR = 0x80; IAP_CMD = cmd;
//       IAP_ADDRL = addr; IAP_ADDRH = addr >> 8;
//       IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;   // 触发
//       if (cmd == 1) *data = IAP_DATA;     // 读
//       else if (cmd == 2) IAP_DATA = *data; // 写
//       IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0;
//   }
//   写需先擦除扇区（cmd=3）。本文件留 TODO 供目标芯片补齐。

u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen) {
    // TODO: 用 IAP 读 addr 处字符串
    (void)addr; (void)out; (void)outlen;
    if (outlen) out[0] = 0;
    return FALSE;
}

u8 fe_port_eeprom_set_str(u16 addr, const char *value) {
    // TODO: 用 IAP 写字符串到 addr
    (void)addr; (void)value;
    return TRUE;
}

u8 fe_port_eeprom_get_u32(u16 addr, u32 *out) {
    // TODO: 读 4 字节（小端）
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
    // TODO: 熵源建议：未初始化 RAM、定时器低字节、ADC 噪声
    for (i = 0; i < len; i++) buf[i] = (u8)(i * 31 + 7);
}

// ============================================================
// 延时
// ============================================================
void fe_port_delay_ms(u32 ms) {
    volatile u32 i;
    for (; ms > 0; ms--)
        for (i = 0; i < FOSC / 1000UL / 4UL; i++) ;   // 近似 1ms
}

// ============================================================
// 寄存器 / 存储空间读写（SDCC 真实实现）
// ============================================================
// SFR 无法用变量寻址，SDCC 中同样用跳转表（sfr 符号由 <8051.h> 提供）。
u8 fe_port_sfr_read(u8 addr) {
    switch (addr) {
        case 0x80: return P0;
        case 0x81: return SP;
        case 0x82: return DPL;
        case 0x83: return DPH;
        case 0x88: return TCON;
        case 0x89: return TMOD;
        case 0x8A: return TL0;
        case 0x8B: return TL1;
        case 0x8C: return TH0;
        case 0x8D: return TH1;
        case 0x90: return P1;
        case 0x98: return SCON;
        case 0x99: return SBUF;
        case 0xA0: return P2;
        case 0xA8: return IE;
        case 0xB0: return P3;
        case 0xB8: return IP;
        case 0xD0: return PSW;
        case 0xE0: return ACC;
        case 0xF0: return B;
        default: return 0;
    }
}

void fe_port_sfr_write(u8 addr, u8 val) {
    switch (addr) {
        case 0x80: P0 = val; break;
        case 0x81: SP = val; break;
        case 0x82: DPL = val; break;
        case 0x83: DPH = val; break;
        case 0x88: TCON = val; break;
        case 0x89: TMOD = val; break;
        case 0x8A: TL0 = val; break;
        case 0x8B: TL1 = val; break;
        case 0x8C: TH0 = val; break;
        case 0x8D: TH1 = val; break;
        case 0x90: P1 = val; break;
        case 0x98: SCON = val; break;
        case 0x99: SBUF = val; break;
        case 0xA0: P2 = val; break;
        case 0xA8: IE = val; break;
        case 0xB0: P3 = val; break;
        case 0xB8: IP = val; break;
        case 0xD0: PSW = val; break;
        case 0xE0: ACC = val; break;
        case 0xF0: B = val; break;
        default: break;
    }
}

u8 fe_port_xram_read(u16 addr) {
    return *((volatile u8 __xdata *)(u16)addr);
}

void fe_port_xram_write(u16 addr, u8 val) {
    *((volatile u8 __xdata *)(u16)addr) = val;
}

// ============================================================
// 芯片信息（SDCC 真实实现）
// ============================================================
void fe_port_chip_info(char *out, u16 outlen) {
    // STC89C52RC 为例；换芯片改此处即可
    fe_snprintf(out, outlen,
                "{\"chip\":\"STC89C52RC\",\"arch\":\"MCS-51\","
                "\"ramBytes\":256,\"flashBytes\":8192,\"freqMHz\":%lu}",
                (unsigned long)(FOSC / 1000000UL));
}