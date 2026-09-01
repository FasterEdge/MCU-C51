// ─────────────────────────────────────────────────────────────
// FasterEdge 开源项目
// Github: https://github.com/FasterEdge
// Gitee:  https://gitee.com/FasterEdge
// ─────────────────────────────────────────────────────────────
// register.c — 注册全部 Data / Ability 到全局 Atom（C51/8051 版）
// 注意：C51 对 C99 复合字面量支持有限，模块全部用显式静态变量。
#include "fe.h"
#include "fe_ability.h"
#include "fe_data.h"
#include <string.h>

// ============================================================
// 模块实例
// ============================================================
static role_ability_t     g_role;
static time_ability_t     g_time;
static onekey_ability_t   g_onekey;
static serial_ability_t   g_serial;
static modbus_ability_t   g_modbus;

static config_data_t      g_config_data;

// ============================================================
// Data 命令表
// ============================================================
static const fe_cmd_t s_base_cmds[] = {
    {"logo", data_base_dispatch},
    {"info", data_base_dispatch},
};
static const fe_cmd_t s_config_cmds[] = {
    {"get", data_config_dispatch},
    {"set", data_config_dispatch},
    {"delete", data_config_dispatch},
    {"list", data_config_dispatch},
    {"snapshot", data_config_dispatch},
};
static const fe_cmd_t s_chip_cmds[] = {
    {"info", data_chip_dispatch},
};

// ============================================================
// Ability 命令表
// ============================================================
static const fe_cmd_t s_ability_base_cmds[] = {
    {"list_data_names", ability_base_dispatch},
    {"list_ability_names", ability_base_dispatch},
};
static const fe_cmd_t s_role_cmds[] = {
    {"describe", ability_role_dispatch},
    {"set_role", ability_role_dispatch},
    {"get_role", ability_role_dispatch},
};
static const fe_cmd_t s_time_cmds[] = {
    {"sync_manual", ability_time_dispatch},
    {"sync_system", ability_time_dispatch},
    {"get_time", ability_time_dispatch},
    {"configure_run", ability_time_dispatch},
};
static const fe_cmd_t s_onekey_cmds[] = {
    {"issue_token", ability_onekey_dispatch},
    {"verify_token", ability_onekey_dispatch},
    {"revoke_all", ability_onekey_dispatch},
    {"list_tokens", ability_onekey_dispatch},
    {"status", ability_onekey_dispatch},
    {"rotate", ability_onekey_dispatch},
};
static const fe_cmd_t s_serial_cmds[] = {
    {"open", ability_serial_dispatch},
    {"close", ability_serial_dispatch},
    {"write", ability_serial_dispatch},
    {"read", ability_serial_dispatch},
    {"is_open", ability_serial_dispatch},
    {"set_config", ability_serial_dispatch},
    {"get_config", ability_serial_dispatch},
    {"list_ports", ability_serial_dispatch},
};
static const fe_cmd_t s_modbus_cmds[] = {
    {"set_unit_id", ability_modbus_dispatch},
    {"get_unit_id", ability_modbus_dispatch},
    {"read_holding", ability_modbus_dispatch},
    {"read_input", ability_modbus_dispatch},
    {"read_coils", ability_modbus_dispatch},
    {"read_discrete", ability_modbus_dispatch},
    {"write_holding", ability_modbus_dispatch},
    {"write_coil", ability_modbus_dispatch},
};
static const fe_cmd_t s_reg_cmds[] = {
    {"read_sfr", ability_reg_dispatch},
    {"write_sfr", ability_reg_dispatch},
    {"read_xram", ability_reg_dispatch},
    {"write_xram", ability_reg_dispatch},
    {"info", ability_reg_dispatch},
};
static const fe_cmd_t s_gpio_cmds[] = {
    {"mode", ability_gpio_dispatch},
    {"write", ability_gpio_dispatch},
    {"read", ability_gpio_dispatch},
    {"info", ability_gpio_dispatch},
};

// ============================================================
// 模块定义（显式静态变量，C51 兼容）
// ============================================================
static fe_module_t s_data_base   = { "BaseData",   "框架元信息", s_base_cmds,     0, NULL,          data_base_dispatch };
static fe_module_t s_data_config = { "ConfigData", "KV 配置(EEPROM)", s_config_cmds, 0, &g_config_data, data_config_dispatch };
static fe_module_t s_data_chip   = { "ChipData",   "芯片信息(MCU 专有)", s_chip_cmds, 0, NULL,          data_chip_dispatch };

static fe_module_t s_ability_base   = { "BaseAbility",   "基础",   s_ability_base_cmds, 0, NULL,          ability_base_dispatch };
static fe_module_t s_ability_role   = { "RoleAbility",   "角色",   s_role_cmds,         0, &g_role,       ability_role_dispatch };
static fe_module_t s_ability_time   = { "TimeAbility",   "时间",   s_time_cmds,         0, &g_time,       ability_time_dispatch };
static fe_module_t s_ability_onekey = { "OneKeyAbility", "一键令牌", s_onekey_cmds,     0, &g_onekey,     ability_onekey_dispatch };
static fe_module_t s_ability_serial = { "SerialAbility", "串口",   s_serial_cmds,       0, &g_serial,     ability_serial_dispatch };
static fe_module_t s_ability_modbus = { "ModbusAbility", "Modbus", s_modbus_cmds,       0, &g_modbus,     ability_modbus_dispatch };
static fe_module_t s_ability_reg    = { "RegAbility",    "寄存器操作(专有)", s_reg_cmds, 0, NULL,      ability_reg_dispatch };
static fe_module_t s_ability_gpio   = { "GpioAbility",   "端口 GPIO(专有)", s_gpio_cmds, 0, NULL,      ability_gpio_dispatch };

// ============================================================
// 注册 Data
// ============================================================
void fe_register_all_data(fe_atom_t *atom) {
    s_data_base.cmd_count   = (u8)(sizeof(s_base_cmds)   / sizeof(s_base_cmds[0]));
    s_data_config.cmd_count = (u8)(sizeof(s_config_cmds) / sizeof(s_config_cmds[0]));
    s_data_chip.cmd_count   = (u8)(sizeof(s_chip_cmds)   / sizeof(s_chip_cmds[0]));
    g_config_data.base_addr = 0x0000;
    fe_register_data(atom, &s_data_base);
    fe_register_data(atom, &s_data_config);
    fe_register_data(atom, &s_data_chip);
}

// ============================================================
// 注册 Ability
// ============================================================
void fe_register_all_abilities(fe_atom_t *atom) {
    s_ability_base.cmd_count   = (u8)(sizeof(s_ability_base_cmds) / sizeof(s_ability_base_cmds[0]));
    s_ability_role.cmd_count   = (u8)(sizeof(s_role_cmds)         / sizeof(s_role_cmds[0]));
    s_ability_time.cmd_count   = (u8)(sizeof(s_time_cmds)         / sizeof(s_time_cmds[0]));
    s_ability_onekey.cmd_count = (u8)(sizeof(s_onekey_cmds)       / sizeof(s_onekey_cmds[0]));
    s_ability_serial.cmd_count = (u8)(sizeof(s_serial_cmds)       / sizeof(s_serial_cmds[0]));
    s_ability_modbus.cmd_count = (u8)(sizeof(s_modbus_cmds)       / sizeof(s_modbus_cmds[0]));
    s_ability_reg.cmd_count    = (u8)(sizeof(s_reg_cmds)          / sizeof(s_reg_cmds[0]));
    s_ability_gpio.cmd_count   = (u8)(sizeof(s_gpio_cmds)         / sizeof(s_gpio_cmds[0]));

    g_serial.open = FALSE;
    g_serial.baud = 115200;
    g_serial.port = 0;
    g_modbus.unit_id = 1;

    fe_register_ability(atom, &s_ability_base);
    fe_register_ability(atom, &s_ability_role);
    fe_register_ability(atom, &s_ability_time);
    fe_register_ability(atom, &s_ability_onekey);
    fe_register_ability(atom, &s_ability_serial);
    fe_register_ability(atom, &s_ability_modbus);
    fe_register_ability(atom, &s_ability_reg);
    fe_register_ability(atom, &s_ability_gpio);
}

// ============================================================
// 初始化全部
// ============================================================
void fe_init_all(void) {
    fe_atom_t *atom = fe_global_atom();
    memset(atom, 0, sizeof(*atom));
    fe_register_all_data(atom);
    fe_register_all_abilities(atom);
}