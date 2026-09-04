/* FasterEdge 开源项目
 * GitHub: https://github.com/FasterEdge
 * Gitee:  https://gitee.com/FasterEdge
 */
// fe_port.c — FasterEdge MCU 平台移植层实现（C51/8051 版，Keil C51 工具链）
// 目标芯片：AT89C52（uvproj Device）。外接 24Cxx I2C EEPROM（默认 P1.0=SCL, P1.1=SDA）。
// 换用 STC 系列（内置 IAP）时，按文件末尾参考把 EEPROM 换成 IAP 实现即可。
#include "fe_port.h"
#include <reg52.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// ============================================================
// 格式化输出
// ============================================================
// Keil C51 无标准 snprintf/vsnprintf：原实现先 vsprintf 到固定
// tmp[128] 再截断 —— 格式化结果超 127 字节时 vsprintf 先写穿栈
// 缓冲（截断逻辑形同虚设），属真实缓冲区溢出缺陷。
// 改为手写有界格式化核心：逐格式符解析，写满缓冲即停。
// 支持 %d %i %u %x %X %c %s 及 %ld/%lu/%lx、宽度/零填充/左对齐。

// 追加 len 字节到 dst，超界返回 -1。
static int fe_fmt_put(char *dst, size_t cap, size_t *pos, const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (*pos + 1 >= cap) return -1;
        dst[(*pos)++] = s[i];
    }
    return 0;
}

// 无符号整数转十进制/十六进制字符串，返回长度（负值由调用方处理）。
static int fe_fmt_uint(char *out, size_t ocap, unsigned long v, unsigned base, int upper) {
    char d[24];
    int i = 0;
    if (v == 0) {
        if (ocap < 2) return -1;
        out[0] = '0';
        out[1] = 0;
        return 1;
    }
    while (v > 0 && i < (int)sizeof(d) - 1) {
        unsigned digit = (unsigned)(v % base);
        d[i++] = (char)(digit < 10 ? (char)('0' + digit)
                                   : (char)((upper ? 'A' : 'a') + (int)digit - 10));
        v /= base;
    }
    if ((size_t)i >= ocap) return -1;
    { int j; for (j = 0; j < i; j++) out[j] = d[i - 1 - j]; }
    out[i] = 0;
    return i;
}

static int fe_vsnprintf_bounded(char *dst, size_t cap, const char *fmt, va_list ap) {
    size_t pos = 0;
    const char *p = fmt;
    if (cap == 0) return 0;
    while (*p != 0 && pos + 1 < cap) {
        char conv;
        int left = 0, zero = 0, width = 0, lng = 0;
        char num[24];
        int nlen;
        if (*p != '%') { dst[pos++] = *p++; continue; }
        p++;
        if (*p == '-') { left = 1; p++; }
        if (*p == '0') { zero = 1; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        if (*p == 'l') { lng = 1; p++; }
        conv = *p++;
        switch (conv) {
        case 'd':
        case 'i': {
            long v = lng ? va_arg(ap, long) : (long)va_arg(ap, int);
            unsigned long uv = (v < 0) ? (unsigned long)(-v) : (unsigned long)v;
            char body[24];
            int bl = fe_fmt_uint(body, sizeof(body), uv, 10, 0);
            int i2;
            if (bl < 0) return -1;
            if (v < 0) {
                if ((size_t)(bl + 1) >= sizeof(num)) return -1;
                num[0] = '-';
                for (i2 = 0; i2 < bl; i2++) num[i2 + 1] = body[i2];
                nlen = bl + 1;
            } else {
                for (i2 = 0; i2 < bl; i2++) num[i2] = body[i2];
                nlen = bl;
            }
            break;
        }
        case 'u': {
            unsigned long v = lng ? va_arg(ap, unsigned long)
                                  : (unsigned long)va_arg(ap, unsigned int);
            nlen = fe_fmt_uint(num, sizeof(num), v, 10, 0);
            if (nlen < 0) return -1;
            break;
        }
        case 'x':
        case 'X': {
            unsigned long v = lng ? va_arg(ap, unsigned long)
                                  : (unsigned long)va_arg(ap, unsigned int);
            nlen = fe_fmt_uint(num, sizeof(num), v, 16, conv == 'X');
            if (nlen < 0) return -1;
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (fe_fmt_put(dst, cap, &pos, &c, 1) != 0) return -1;
            continue;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            size_t sl;
            if (s == 0) s = "(null)";
            for (sl = 0; s[sl] != 0; sl++) {
                if (pos + 1 >= cap) return -1;
                dst[pos++] = s[sl];
            }
            continue;
        }
        case '%':
            if (fe_fmt_put(dst, cap, &pos, "%", 1) != 0) return -1;
            continue;
        default: {
            char cbuf[2];
            if (fe_fmt_put(dst, cap, &pos, "%", 1) != 0) return -1;
            cbuf[0] = conv;
            if (fe_fmt_put(dst, cap, &pos, cbuf, 1) != 0) return -1;
            continue;
        }
        }
        /* 数字：宽度/零填充/左对齐 */
        if (nlen < width) {
            int pad = width - nlen;
            int i2;
            if (!left && zero && num[0] == '-') {
                /* 负号在前，零填充在数字体前：-0012 */
                if (fe_fmt_put(dst, cap, &pos, num, 1) != 0) return -1;
                for (i2 = 0; i2 < pad; i2++) {
                    if (pos + 1 >= cap) return -1;
                    dst[pos++] = '0';
                }
                if (fe_fmt_put(dst, cap, &pos, num + 1, (size_t)(nlen - 1)) != 0) return -1;
            } else {
                if (!left) {
                    for (i2 = 0; i2 < pad; i2++) {
                        if (pos + 1 >= cap) return -1;
                        dst[pos++] = (char)(zero ? '0' : ' ');
                    }
                }
                if (fe_fmt_put(dst, cap, &pos, num, (size_t)nlen) != 0) return -1;
                if (left) {
                    for (i2 = 0; i2 < pad; i2++) {
                        if (pos + 1 >= cap) return -1;
                        dst[pos++] = ' ';
                    }
                }
            }
        } else {
            if (fe_fmt_put(dst, cap, &pos, num, (size_t)nlen) != 0) return -1;
        }
    }
    dst[pos] = 0;
    return (int)pos;
}

int fe_snprintf(char *buf, u16 size, const char *fmt, ...) {
    va_list ap;
    int n;
    if (size == 0) return 0;
    va_start(ap, fmt);
    n = fe_vsnprintf_bounded(buf, size, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    buf[size - 1] = 0; /* 保证终止 */
    return n;
}

// ============================================================
// 串口（UART0，定时器 1 作波特率）
// ============================================================
static fe_port_uart_rx_cb_t g_rx_cb = NULL;
static void *g_rx_user = NULL;

// 波特率重载值：TH1 = 256 - FOSC/(12*32*baud)
static u8 baud_reload(u32 baud) {
    u32 t = FOSC / 12UL / 32UL;
    u8 v = 0;
    if (baud > 0) {
        t /= baud;
        if (t < 256) v = (u8)(256 - t);
        else v = 1;
    }
    return v;
}

void fe_port_uart_init(u8 port, u32 baud, fe_port_uart_rx_cb_t rx_cb, void *user) {
    u8 reload;
    (void)port;
    g_rx_cb = rx_cb;
    g_rx_user = user;
    reload = baud_reload(baud);
    TMOD = (TMOD & 0x0F) | 0x20;   // 定时器1 模式2（8 位自动重载）
    TH1 = reload;
    TL1 = reload;
    SCON = 0x50;                   // 模式1，REN=1 允许接收
    TR1 = 1;                       // 启动波特率定时器
    if (rx_cb) { ES = 1; EA = 1; } // 需要回调时开串口中断
}

// 串口接收中断（中断号 4）
void fe_port_uart_isr(void) interrupt 4 using 1 {
    u8 b;
    if (!RI) return;
    RI = 0;
    b = SBUF;
    if (g_rx_cb) g_rx_cb(b, g_rx_user);
}

u16 fe_port_uart_write(u8 port, const u8 *data, u16 len) {
    u16 i;
    (void)port;
    for (i = 0; i < len; i++) {
        while (!TI);
        TI = 0;
        SBUF = data[i];
    }
    return len;
}

u8 fe_port_uart_available(u8 port) {
    (void)port;
    return RI ? TRUE : FALSE;
}

int fe_port_uart_read(u8 port) {
    (void)port;
    if (!RI) return -1;
    RI = 0;
    return SBUF;
}

void fe_port_uart_close(u8 port) {
    (void)port;
    ES = 0;
}

// ============================================================
// EEPROM（外接 24Cxx I2C，512B/页，2 字节地址）
// 默认引脚：P1.0=SCL, P1.1=SDA（可按硬件接线修改）
// ============================================================
// Keil C51：sbit 声明 I2C 引脚（默认 P1.0=SCL / P1.1=SDA，按硬件接线修改）
sbit FE_I2C_SCL = P1^0;
sbit FE_I2C_SDA = P1^1;
#define FE_I2C_DEVADDR 0xA0      // 24Cxx 设备地址（A0-A2 接地）

static void i2c_delay(void) { unsigned char i; for (i = 0; i < 5; i++) ; }

static void i2c_start(void) {
    FE_I2C_SDA = 1; FE_I2C_SCL = 1; i2c_delay();
    FE_I2C_SDA = 0; i2c_delay();
    FE_I2C_SCL = 0;
}

static void i2c_stop(void) {
    FE_I2C_SDA = 0; FE_I2C_SCL = 1; i2c_delay();
    FE_I2C_SDA = 1; i2c_delay();
}

// 发送一字节，返回 ACK（0=成功）
static u8 i2c_write_byte(u8 b) {
    u8 i, ack;
    for (i = 0; i < 8; i++) {
        FE_I2C_SDA = (b & 0x80) ? 1 : 0;
        b <<= 1;
        FE_I2C_SCL = 1; i2c_delay();
        FE_I2C_SCL = 0; i2c_delay();
    }
    FE_I2C_SDA = 1;              // 释放 SDA 读 ACK
    FE_I2C_SCL = 1; i2c_delay();
    ack = FE_I2C_SDA;
    FE_I2C_SCL = 0; i2c_delay();
    return ack;
}

// 读一字节，ack=1 时主机应答（最后一字节应传 0）
static u8 i2c_read_byte(u8 ack) {
    u8 i, b = 0;
    FE_I2C_SDA = 1;
    for (i = 0; i < 8; i++) {
        FE_I2C_SCL = 1; i2c_delay();
        b = (u8)((b << 1) | (FE_I2C_SDA ? 1 : 0));
        FE_I2C_SCL = 0; i2c_delay();
    }
    FE_I2C_SDA = ack ? 0 : 1;
    FE_I2C_SCL = 1; i2c_delay();
    FE_I2C_SCL = 0; i2c_delay();
    FE_I2C_SDA = 1;
    return b;
}

// 写一个字节：start, devaddr, hi, lo, data, stop；返回 ACK
static u8 eeprom_write_byte(u16 addr, u8 val) {
    u8 ok;
    i2c_start();
    ok = i2c_write_byte(FE_I2C_DEVADDR);
    ok |= i2c_write_byte((u8)(addr >> 8));
    ok |= i2c_write_byte((u8)addr);
    ok |= i2c_write_byte(val);
    i2c_stop();
    return ok ? FALSE : TRUE;
}

// 读一个字节
static u8 eeprom_read_byte(u16 addr) {
    u8 b;
    i2c_start();
    i2c_write_byte(FE_I2C_DEVADDR);
    i2c_write_byte((u8)(addr >> 8));
    i2c_write_byte((u8)addr);
    i2c_start();
    i2c_write_byte(FE_I2C_DEVADDR | 0x01);
    b = i2c_read_byte(0);
    i2c_stop();
    return b;
}

u8 fe_port_eeprom_get_str(u16 addr, char *out, u16 outlen) {
    u16 i;
    if (!out || outlen == 0) return FALSE;
    for (i = 0; i + 1 < outlen; i++) {
        u8 c = eeprom_read_byte((u16)(addr + i));
        out[i] = (char)c;
        if (c == 0) return TRUE;
    }
    out[outlen - 1] = 0;
    return TRUE;
}

u8 fe_port_eeprom_set_str(u16 addr, const char *value) {
    u16 i;
    for (i = 0; value[i]; i++) {
        if (!eeprom_write_byte((u16)(addr + i), (u8)value[i])) return FALSE;
        fe_port_delay_ms(5);     // 24Cxx 内部写周期
    }
    return TRUE;
}

u8 fe_port_eeprom_get_u32(u16 addr, u32 *out) {
    u8 i;
    u32 v = 0;
    if (!out) return FALSE;
    for (i = 0; i < 4; i++)
        v |= (u32)eeprom_read_byte((u16)(addr + i)) << (8 * i);
    *out = v;
    return TRUE;
}

u8 fe_port_eeprom_set_u32(u16 addr, u32 value) {
    u8 i;
    for (i = 0; i < 4; i++)
        if (!eeprom_write_byte((u16)(addr + i), (u8)(value >> (8 * i)))) return FALSE;
    return TRUE;
}

// ============================================================
// 系统时间——定时器 0（模式 1）50ms 中断计数
// ============================================================
static volatile u32 s_epoch_base;
static volatile u8  s_second_count;
static volatile u8  s_timer0_ready;

#define TIMER0_RELOAD (65536UL - FOSC / 12UL / 20UL)   // 50ms @12T

void fe_port_timer0_isr(void) interrupt 1 using 1 {
    TH0 = (u8)(TIMER0_RELOAD >> 8);
    TL0 = (u8)TIMER0_RELOAD;
    if (++s_second_count >= 20) {   // 20 * 50ms = 1s
        s_second_count = 0;
        s_epoch_base++;
    }
}

static void timer0_start(void) {
    if (s_timer0_ready) return;
    s_timer0_ready = 1;
    s_second_count = 0;
    TMOD = (TMOD & 0xF0) | 0x01;   // 定时器0 模式1（16 位）
    TH0 = (u8)(TIMER0_RELOAD >> 8);
    TL0 = (u8)TIMER0_RELOAD;
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
    state ^= (u32)TL0 << 8 | TH0;
    for (i = 0; i < len; i++) {
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
        for (i = 0; i < 1000; i++) ;
}

// ============================================================
// 寄存器 / 存储空间读写（跳转表）
// ============================================================
u8 fe_port_sfr_read(u8 addr) {
    switch (addr) {
        case 0x80: return P0;   case 0x90: return P1;
        case 0xA0: return P2;   case 0xB0: return P3;
        case 0x88: return TCON; case 0x98: return SCON;
        case 0x8A: return TL0;  case 0x8B: return TL1;
        case 0x8C: return TH0;  case 0x8D: return TH1;
        case 0xA8: return IE;   case 0xB8: return IP;
        case 0xD0: return PSW;  case 0xE0: return ACC;
        case 0xF0: return B;
        default: return 0;
    }
}

void fe_port_sfr_write(u8 addr, u8 val) {
    switch (addr) {
        case 0x80: P0 = val; break;   case 0x90: P1 = val; break;
        case 0xA0: P2 = val; break;   case 0xB0: P3 = val; break;
        case 0x8A: TL0 = val; break;  case 0x8B: TL1 = val; break;
        case 0x8C: TH0 = val; break;  case 0x8D: TH1 = val; break;
        default: break;
    }
}

u8 fe_port_xram_read(u16 addr) {
    return *(volatile u8 xdata *)addr;
}

void fe_port_xram_write(u16 addr, u8 val) {
    *(volatile u8 xdata *)addr = val;
}

// ============================================================
// 芯片信息
// ============================================================
void fe_port_chip_info(char *out, u16 outlen) {
    fe_snprintf(out, outlen,
        "{\"chip\":\"AT89C52\",\"arch\":\"MCS-51\","
        "\"ramBytes\":256,\"flashBytes\":8192,\"eepromBytes\":0,\"freqMHz\":12}");
}

/* 参考：换用 STC 系列（内置 IAP）时把 EEPROM 替换为：
 *   #include "stc15.h"
 *   void iap_idle(void)      { IAP_CONTR = 0; IAP_CMD = 0; IAP_TRIG = 0; IAP_ADDRH = 0; IAP_ADDRL = 0; }
 *   u8    iap_read(u16 addr) { u8 d; IAP_CONTR = 0x80; IAP_CMD = 1; IAP_ADDRL = addr; IAP_ADDRH = addr>>8;
 *                              IAP_TRIG = 0x5A; IAP_TRIG = 0xA5; d = IAP_DATA; iap_idle(); return d; }
 *   void iap_write(u16 addr, u8 d) { IAP_CONTR = 0x80; IAP_CMD = 2; ... IAP_DATA = d; ... }
 *   void iap_erase(u16 addr)       { IAP_CONTR = 0x80; IAP_CMD = 3; ... }
 * 写字符串按扇区：先读回 512B 扇区、改字节、擦除、重写。
 */
