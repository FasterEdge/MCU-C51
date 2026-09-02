// FasterEdge 开源项目 - Github: https://github.com/FasterEdge - Gitee: https://gitee.com/FasterEdge
// ability_reg.c — RegAbility 实现（C51/8051 版，MCU 专有）
// MCU 专有能力：SFR / XRAM 读写。
//   read_sfr <0x80-0xFF>       读特殊功能寄存器
//   write_sfr <addr>,<value>   写特殊功能寄存器
//   read_xram <0x0000-0xFFFF>  读外部扩展 RAM
//   write_xram <addr>,<value>  写外部扩展 RAM
//   info                       说明
// 注意：请谨慎使用，误写寄存器可能导致系统异常。
#include "fe_ability.h"
#include "fe_port.h"
#include <string.h>

static u8 hex_val(char c) {
    if (c >= '0' && c <= '9') return (u8)(c - '0');
    if (c >= 'a' && c <= 'f') return (u8)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (u8)(c - 'A' + 10);
    return 0xFF;
}

// 解析十六进制（支持 0x 前缀）；失败返回 0，*ok=FALSE
static u32 parse_hex(const char *s, u8 *ok) {
    u32 v = 0;
    u8 d;
    *ok = FALSE;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (*s == 0) return 0;
    while (*s) {
        d = hex_val(*s);
        if (d == 0xFF) return 0;
        v = (v << 4) | d;
        s++;
    }
    *ok = TRUE;
    return v;
}

fe_output_t ability_reg_dispatch(void *inst, const char *act, const char *args) {
    char tmp[40];
    char *comma;
    u32 addr;
    u8 ok;

    (void)inst;
    fe_snprintf(tmp, sizeof(tmp), "%s", args ? args : "");

    if (strcmp(act, "read_sfr") == 0) {
        addr = parse_hex(tmp, &ok);
        if (!ok || addr > 0xFF) return fe_err(act, "bad sfr address (0x80-0xFF)");
        {
            u8 v = fe_port_sfr_read((u8)addr);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"addr\":\"0x%02X\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)addr, (unsigned)v, (unsigned)v);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "write_sfr") == 0) {
        comma = strchr(tmp, ',');
        if (!comma) return fe_err(act, "bad format, expect addr,value");
        *comma = 0;
        addr = parse_hex(tmp, &ok);
        if (!ok || addr > 0xFF) return fe_err(act, "bad sfr address (0x80-0xFF)");
        {
            u32 val = parse_hex(comma + 1, &ok);
            if (!ok || val > 0xFF) return fe_err(act, "bad value (0x00-0xFF)");
            fe_port_sfr_write((u8)addr, (u8)val);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"addr\":\"0x%02X\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)addr, (unsigned)val, (unsigned)val);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "read_xram") == 0) {
        addr = parse_hex(tmp, &ok);
        if (!ok || addr > 0xFFFF) return fe_err(act, "bad xram address (0x0000-0xFFFF)");
        {
            u8 v = fe_port_xram_read((u16)addr);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"addr\":\"0x%04X\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)addr, (unsigned)v, (unsigned)v);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "write_xram") == 0) {
        comma = strchr(tmp, ',');
        if (!comma) return fe_err(act, "bad format, expect addr,value");
        *comma = 0;
        addr = parse_hex(tmp, &ok);
        if (!ok || addr > 0xFFFF) return fe_err(act, "bad xram address (0x0000-0xFFFF)");
        {
            u32 val = parse_hex(comma + 1, &ok);
            if (!ok || val > 0xFF) return fe_err(act, "bad value (0x00-0xFF)");
            fe_port_xram_write((u16)addr, (u8)val);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"addr\":\"0x%04X\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)addr, (unsigned)val, (unsigned)val);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "info") == 0) {
        return fe_ok(act,
            "{\"ability\":\"RegAbility\",\"desc\":\"MCU SFR/XRAM 读写\","
            "\"sfr\":\"0x80-0xFF\",\"xram\":\"0x0000-0xFFFF\"}");
    }
    return fe_err(act, "unsupported command");
}
