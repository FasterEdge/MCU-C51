// data_config.c — ConfigData 实现（C51/8051 版）
// 扁平点号路径 KV 配置（EEPROM 持久化）：get / set / delete / list / snapshot
// 布局：自 base_addr 起，最多 CFG_SLOTS 个条目，每条目 key[16] + value[32]。
#include "fe_data.h"
#include "fe_port.h"
#include <string.h>

#define CFG_SLOTS     16
#define CFG_KEY_LEN   16
#define CFG_VAL_LEN   32
#define CFG_ENTRY     (CFG_KEY_LEN + CFG_VAL_LEN)   // 48 字节

static void norm_key(const char *in, char *out, u16 outlen) {
    u16 n = 0;
    const char *p;
    for (p = in; *p && n + 1 < outlen; p++) {
        char c = *p;
        if (c == '.' || c == '/') c = '_';
        out[n++] = c;
    }
    out[n] = 0;
}

// 查找条目：存在返回槽位号，不存在返回 -1
static int find_slot(u16 base, const char *key) {
    int i;
    for (i = 0; i < CFG_SLOTS; i++) {
        u16 addr = base + (u16)(i * CFG_ENTRY);
        char k[CFG_KEY_LEN];
        if (!fe_port_eeprom_get_str(addr, k, sizeof(k))) continue;
        if (strcmp(k, key) == 0) return i;
    }
    return -1;
}

fe_output_t data_config_dispatch(void *inst, const char *act, const char *args) {
    config_data_t *self = (config_data_t *)inst;
    u16 base = self->base_addr;

    if (strcmp(act, "get") == 0) {
        char key[CFG_KEY_LEN];
        int slot;
        char val[CFG_VAL_LEN];
        char out[80];
        if (!args || !args[0]) return fe_err(act, "missing key");
        norm_key(args, key, sizeof(key));
        slot = find_slot(base, key);
        if (slot < 0) return fe_ok(act, "{}");
        fe_port_eeprom_get_str(base + (u16)(slot * CFG_ENTRY) + CFG_KEY_LEN,
                               val, sizeof(val));
        fe_snprintf(out, sizeof(out), "{\"%s\":\"%s\"}", key, val);
        return fe_ok(act, out);
    }
    if (strcmp(act, "set") == 0) {
        char buf[64];
        char *eq;
        char key[CFG_KEY_LEN];
        int slot, i;
        u16 addr;
        if (!args || !args[0]) return fe_err(act, "bad format, expect key=value");
        fe_snprintf(buf, sizeof(buf), "%s", args);
        eq = strchr(buf, '=');
        if (!eq) return fe_err(act, "bad format, expect key=value");
        *eq = 0;
        norm_key(buf, key, sizeof(key));
        slot = find_slot(base, key);
        if (slot < 0) {   // 找空槽
            for (i = 0; i < CFG_SLOTS; i++) {
                char k[CFG_KEY_LEN];
                if (!fe_port_eeprom_get_str(base + (u16)(i * CFG_ENTRY), k, sizeof(k)) ||
                    k[0] == 0) { slot = i; break; }
            }
            if (slot < 0) return fe_err(act, "config full");
            fe_port_eeprom_set_str(base + (u16)(slot * CFG_ENTRY), key);
        }
        addr = base + (u16)(slot * CFG_ENTRY) + CFG_KEY_LEN;
        fe_port_eeprom_set_str(addr, eq + 1);
        return fe_ok(act, "saved");
    }
    if (strcmp(act, "delete") == 0) {
        char key[CFG_KEY_LEN];
        int slot;
        if (!args || !args[0]) return fe_err(act, "missing key");
        norm_key(args, key, sizeof(key));
        slot = find_slot(base, key);
        if (slot >= 0)
            fe_port_eeprom_set_str(base + (u16)(slot * CFG_ENTRY), "");  // 清空 key 作删除标记
        return fe_ok(act, "deleted");
    }
    if (strcmp(act, "list") == 0) {
        char out[96];
        u16 n = 0;
        int i;
        out[0] = 0;
        for (i = 0; i < CFG_SLOTS; i++) {
            char k[CFG_KEY_LEN];
            u16 addr = base + (u16)(i * CFG_ENTRY);
            if (!fe_port_eeprom_get_str(addr, k, sizeof(k)) || k[0] == 0) continue;
            if (n) out[n++] = ',';
            n += (u16)fe_snprintf(out + n, sizeof(out) - n, "\"%s\"", k);
        }
        {
            char full[120];
            fe_snprintf(full, sizeof(full), "{\"keys\":[%s]}", out);
            return fe_ok(act, full);
        }
    }
    if (strcmp(act, "snapshot") == 0) {
        char out[600];
        u16 n = 0;
        int i;
        out[0] = 0;
        n += (u16)fe_snprintf(out + n, sizeof(out) - n, "{\"snapshot\":{");
        for (i = 0; i < CFG_SLOTS; i++) {
            char k[CFG_KEY_LEN], v[CFG_VAL_LEN];
            u16 addr = base + (u16)(i * CFG_ENTRY);
            if (!fe_port_eeprom_get_str(addr, k, sizeof(k)) || k[0] == 0) continue;
            fe_port_eeprom_get_str(addr + CFG_KEY_LEN, v, sizeof(v));
            if (n + 6 >= sizeof(out)) break;
            if (out[n - 1] != '{') out[n++] = ',';
            n += (u16)fe_snprintf(out + n, sizeof(out) - n, "\"%s\":\"%s\"", k, v);
        }
        n += (u16)fe_snprintf(out + n, sizeof(out) - n, "}}");
        return fe_ok(act, out);
    }
    return fe_err(act, "unsupported command");
}