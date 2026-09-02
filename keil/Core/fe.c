// FasterEdge 开源项目 - Github: https://github.com/FasterEdge - Gitee: https://gitee.com/FasterEdge
// fe.c — FasterEdge MCU 核心框架实现（C51/8051 版）
// 注意：输出格式化统一使用 fe_snprintf（在 fe_port.h 声明、fe_port.c 提供），
// 因为 Keil C51 标准库无 snprintf，且对 %ld/%lu 支持有限。
#include "fe.h"
#include "fe_port.h"
#include <string.h>

// ============================================================
// 注册
// ============================================================
void fe_register_data(fe_atom_t *atom, const fe_module_t *mod) {
    if (atom->data_count >= FE_MAX_MODULES) return;
    atom->data[atom->data_count++] = *mod;
}

void fe_register_ability(fe_atom_t *atom, const fe_module_t *mod) {
    if (atom->ability_count >= FE_MAX_MODULES) return;
    atom->ability[atom->ability_count++] = *mod;
}

// ============================================================
// 名称列表
// ============================================================
void fe_list_data_names(const fe_atom_t *atom, char *out, u16 outlen) {
    u16 n = 0;
    u8 i;
    out[0] = 0;
    for (i = 0; i < atom->data_count && n < outlen; i++) {
        int w = fe_snprintf(out + n, outlen - n, "%s%s", i ? "," : "", atom->data[i].name);
        if (w < 0) break;
        n += (u16)w;
    }
}

void fe_list_ability_names(const fe_atom_t *atom, char *out, u16 outlen) {
    u16 n = 0;
    u8 i;
    out[0] = 0;
    for (i = 0; i < atom->ability_count && n < outlen; i++) {
        int w = fe_snprintf(out + n, outlen - n, "%s%s", i ? "," : "", atom->ability[i].name);
        if (w < 0) break;
        n += (u16)w;
    }
}

// ============================================================
// 命令路由
// ============================================================
fe_output_t fe_execute(fe_atom_t *atom, const char *target, const char *act, const char *args) {
    u8 isData = FALSE;
    const char *base = target;
    if (strncmp(target, "data_", 5) == 0) { isData = TRUE; base = target + 5; }
    else if (strncmp(target, "ability_", 8) == 0) base = target + 8;

    u8 count = isData ? atom->data_count : atom->ability_count;
    const fe_module_t *mods = isData ? atom->data : atom->ability;
    u8 m, c;

    for (m = 0; m < count; m++) {
        if (strcmp(mods[m].name, base) == 0) {
            for (c = 0; c < mods[m].cmd_count; c++) {
                if (strcmp(mods[m].cmds[c].name, act) == 0) {
                    return mods[m].dispatch(mods[m].instance, act, args);
                }
            }
            return fe_err(act, "unsupported command");
        }
    }
    return fe_err(act, isData ? "unknown data" : "unknown ability");
}

// ============================================================
// 输出工具
// ============================================================
fe_output_t fe_ok(const char *name, const char *value) {
    fe_output_t o;
    memset(&o, 0, sizeof(o));
    fe_snprintf(o.name, sizeof(o.name), "%s", name ? name : "");
    fe_snprintf(o.value, sizeof(o.value), "%s", value ? value : "");
    o.ok = TRUE;
    return o;
}

fe_output_t fe_err(const char *name, const char *err) {
    fe_output_t o;
    memset(&o, 0, sizeof(o));
    fe_snprintf(o.name, sizeof(o.name), "%s", name ? name : "");
    fe_snprintf(o.err, sizeof(o.err), "%s", err ? err : "error");
    o.ok = FALSE;
    return o;
}

// ============================================================
// 全局 Atom（在 register.c 中填充）
// ============================================================
static fe_atom_t g_atom;

fe_atom_t *fe_global_atom(void) { return &g_atom; }

void fe_init_all(void);  // 由 register.c 实现
