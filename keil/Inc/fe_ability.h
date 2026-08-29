// fe_ability.h — FasterEdge MCU Ability 模块声明（C51/8051 版）
// 8 位机合理子集：Base / Role / Time / OneKey / Serial / Modbus
// 排除：MQTT（无网络）、EdgeRole（无网络心跳）、ConfigFile（与 Data 重复）
#ifndef FE_ABILITY_H
#define FE_ABILITY_H

#include "fe.h"

// ============================================================
// BaseAbility —— list_data_names / list_ability_names
// ============================================================
fe_output_t ability_base_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// RoleAbility —— describe / set_role / get_role
// ============================================================
typedef struct {
    char role[12];   // edge / cloud / standalone
} role_ability_t;
fe_output_t ability_role_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// TimeAbility —— sync_manual / sync_system / get_time
// （C51 无网络，不含 sync_ntp；configure_run 为 TODO 占位）
// ============================================================
typedef struct {
    u32 manual_epoch;
} time_ability_t;
fe_output_t ability_time_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// OneKeyAbility —— issue_token / verify_token / revoke_all /
//                  list_tokens / status / rotate
// （HMAC-SHA256，密钥存 EEPROM）
// ============================================================
typedef struct {
    char secret[33];  // HMAC 密钥（EEPROM 持久化，32 字节 + NUL）
    u32  seq;         // 令牌序列
} onekey_ability_t;
fe_output_t ability_onekey_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// SerialAbility —— open / close / write / read / is_open /
//                  set_config / get_config / list_ports
// ============================================================
typedef struct {
    u8  open;
    u32 baud;
    u8  port;
} serial_ability_t;
fe_output_t ability_serial_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// ModbusAbility —— set_unit_id / get_unit_id / read_holding /
//                  read_input / read_coils / read_discrete /
//                  write_holding / write_coil
// （RTU 从站寄存器表存 RAM）
// ============================================================
typedef struct {
    u8    unit_id;
    u16   holding_regs[32];
    u16   input_regs[32];
    u8    coils[32];
    u8    discrete_inputs[32];
} modbus_ability_t;
fe_output_t ability_modbus_dispatch(void *inst, const char *act, const char *args);

// ============================================================
// 注册全部 Ability（register.c 调用）
// ============================================================
void fe_register_all_abilities(fe_atom_t *atom);

#endif // FE_ABILITY_H