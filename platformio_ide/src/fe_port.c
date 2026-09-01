// ─────────────────────────────────────────────────────────────
// FasterEdge 开源项目
// Github: https://github.com/FasterEdge
// Gitee:  https://gitee.com/FasterEdge
// ─────────────────────────────────────────────────────────────
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
// STC89C52RC 内置 EEPROM（IAP）寄存器（SDCC：__sfr __at 声明）
// STC89 系列 IAP 寄存器位于 0xE2-0xE7；STC15/STC8 系列为 0xC2-0xC7。
// 换用其他 STC 型号时按数据手册调整地址即可。
// ============================================================
__sfr __at(0xE2) IAP_DATA;
__sfr __at(0xE3) IAP_ADDRH;
__sfr __at(0xE4) IAP_ADDRL;
__sfr __at(0xE5) IAP_CMD;
__sfr __at(0xE6) IAP_TRIG;
__sfr __at(0xE7) IAP_CONTR;

// 512B 数据扇区（STC89 系列数据手册）
#define FE_IAP_SECTOR 512

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
// EEPROM（存储配置/密钥）——STC89 内置 IAP 实现
// 写前先整扇区擦除（IAP 只能把 1 写为 0），扇区 512B。
// ============================================================
static void iap_idle(void) {
    IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0;
    IAP_ADDRH = 0; IAP_ADDRL = 0;
}

// 读一个字节
static u8 iap_read(u16 addr) {
    u8 d;
    IAP_CONTR = 0x80; IAP_CMD = 1;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    d = IAP_DATA;
    iap_idle();
    return d;
}

// 写一个字节（只允许 1->0；整扇区写入前必须先擦除）
static void iap_write(u16 addr, u8 d) {
    IAP_CONTR = 0x80; IAP_CMD = 2;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_DATA = d;
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    iap_idle();
}

// 擦除一个扇区（addr 为扇区起始地址）
static void iap_erase(u16 addr) {
    IAP_CONTR = 0x80; IAP_CMD = 3;
    IAP_ADDRL = (u8)addr; IAP_ADDRH = (u8)(addr >> 8);
    IAP_TRIG = 0x5A; IAP_TRIG = 0xA5;
    iap_idle();
}

// 读字符串：从 addr 连续读，直至 NUL 或缓冲满
u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen) {
    u16 i;
    if (!out || outlen == 0) return FALSE;
    for (i = 0; i + 1 < outlen; i++) {
        u8 c = iap_read((u16)(addr + i));
        out[i] = (char)c;
        if (c == 0) return TRUE;
    }
    out[outlen - 1] = 0;
    return TRUE;
}

// 写字符串：读回所在扇区、更新、擦除、重写（原子扇区替换）
u8 fe_port_eeprom_set_str(u16 addr, const char *value) {
    u16 base = (u16)(addr & (u16)~(FE_IAP_SECTOR - 1));
    u16 i, off = (u16)(addr - base);
    __xdata u8 page[FE_IAP_SECTOR];  // SDCC：显式 xdata（8051 片上 RAM 仅 256B）
    for (i = 0; i < FE_IAP_SECTOR; i++)
        page[i] = iap_read((u16)(base + i));
    for (i = 0; value[i] && off + i < FE_IAP_SECTOR; i++)
        page[off + i] = (u8)value[i];
    if (off + i < FE_IAP_SECTOR) page[off + i] = 0;
    iap_erase(base);
    for (i = 0; i < FE_IAP_SECTOR; i++)
        iap_write((u16)(base + i), page[i]);
    return TRUE;
}

// 读 u32（小端 4 字节）
u8 fe_port_eeprom_get_u32(u16 addr, u32 *out) {
    u8 i;
    u32 v = 0;
    if (!out) return FALSE;
    for (i = 0; i < 4; i++)
        v |= (u32)iap_read((u16)(addr + i)) << (8 * i);
    *out = v;
    return TRUE;
}

// 写 u32（小端 4 字节）——逐字节写入（同一扇区，事务由调用方保证）
u8 fe_port_eeprom_set_u32(u16 addr, u32 value) {
    u16 base = (u16)(addr & (u16)~(FE_IAP_SECTOR - 1));
    u16 i, off = (u16)(addr - base);
    __xdata u8 page[FE_IAP_SECTOR];
    for (i = 0; i < FE_IAP_SECTOR; i++)
        page[i] = iap_read((u16)(base + i));
    for (i = 0; i < 4; i++)
        page[off + i] = (u8)(value >> (8 * i));
    iap_erase(base);
    for (i = 0; i < FE_IAP_SECTOR; i++)
        iap_write((u16)(base + i), page[i]);
    return TRUE;
}
// ============================================================
// 系统时间——定时器 0（模式 1, 16 位）秒中断计数
// 首次调用时自动初始化；epoch = 基准 + 已计数秒数。
// ============================================================
static volatile u32 s_epoch_base;     // fe_port_time_set 设定的基准
static volatile u16 s_second_count;   // 距最近整秒的 50ms 中断计数
static volatile u8  s_timer0_ready;   // 定时器 0 是否已初始化

// 定时器 0 中断：50ms 一次，20 次 = 1s
static void timer0_isr(void) __interrupt(1) {
    TH0 = (u8)((65536UL - FOSC / 12UL / 20UL) >> 8);
    TL0 = (u8)(65536UL - FOSC / 12UL / 20UL);
    if (++s_second_count >= 20) {
        s_second_count = 0;
        s_epoch_base++;
    }
}

static void timer0_start(void) {
    if (s_timer0_ready) return;
    s_timer0_ready = 1;
    s_second_count = 0;
    TMOD = (TMOD & 0xF0) | 0x01;   // 定时器0 模式1（16 位）
    TH0 = (u8)((65536UL - FOSC / 12UL / 20UL) >> 8);
    TL0 = (u8)(65536UL - FOSC / 12UL / 20UL);
    ET0 = 1;                       // 开定时器0 中断
    EA  = 1;                       // 开总中断
    TR0 = 1;                       // 启动
}

u32 fe_port_time_now(void) {
    timer0_start();
    return s_epoch_base;
}

void fe_port_time_set(u32 epoch) {
    timer0_start();
    s_epoch_base = epoch;
}

// ============================================================
// 随机数
// ============================================================
void fe_port_random_fill(u8 *buf, u16 len) {
    static u32 state = 0xFE51C51u;
    u16 i;
    timer0_start();
    state ^= (u32)TL0 << 8 | TH0;            // 定时器低字节混入熵
    for (i = 0; i < len; i++) {
        // xorshift32
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = (u8)(state >> 24);
    }
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