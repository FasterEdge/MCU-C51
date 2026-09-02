// FasterEdge 开源项目 - Github: https://github.com/FasterEdge - Gitee: https://gitee.com/FasterEdge
// fe_data.h — FasterEdge MCU Data 模块声明（C51/8051 版）
// Base / Config（Keyring 合并入 OneKeyAbility，NetMap 因无网络省略）
#ifndef FE_DATA_H
#define FE_DATA_H

#include "fe.h"

// ============================================================
// BaseData —— logo / info
// ============================================================
fe_output_t data_base_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// ConfigData —— 扁平点号路径 KV 配置（EEPROM）：get/set/delete/list/snapshot
// ============================================================
typedef struct {
    u16 base_addr;   // EEPROM 起始地址
} config_data_t;
fe_output_t data_config_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// ChipData —— MCU 专有·芯片信息：info
// ============================================================
fe_output_t data_chip_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// 注册全部 Data（register.c 调用）
// ============================================================
void fe_register_all_data(fe_atom_t *atom);

#endif // FE_DATA_H
