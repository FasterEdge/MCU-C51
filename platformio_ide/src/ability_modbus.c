// ability_modbus.c — ModbusAbility 实现（C51/8051 版）
// set_unit_id / get_unit_id / read_holding / read_input / read_coils /
// read_discrete / write_holding / write_coil
// 8051 作为 Modbus RTU 从站（寄存器表存于 RAM，各 32 项），
// 帧收发经 fe_port UART。
#include "fe_ability.h"
#include "fe_port.h"
#include <stdlib.h>
#include <string.h>

#define MODBUS_REGS 32

// CRC16 (Modbus)
static u16 modbus_crc(const u8 *data, u16 len) {
    u16 crc = 0xFFFF;
    u16 i;
    u8 b;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (b = 0; b < 8; b++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
    return crc;
}

fe_output_t ability_modbus_dispatch(void *inst, const char *act, const char *args) {
    modbus_ability_t *self = (modbus_ability_t *)inst;

    if (strcmp(act, "set_unit_id") == 0) {
        int id;
        char out[24];
        id = args ? atoi(args) : 0;
        if (id <= 0 || id > 247) return fe_err(act, "invalid unit id");
        self->unit_id = (u8)id;
        fe_snprintf(out, sizeof(out), "unit_id=%d", id);
        return fe_ok(act, out);
    }
    if (strcmp(act, "get_unit_id") == 0) {
        char out[24];
        fe_snprintf(out, sizeof(out), "unit_id=%u", self->unit_id);
        return fe_ok(act, out);
    }

    // 通用参数解析：addr[,count]
    {
        int addr = 0, count = 1;
        if (args && args[0]) {
            char buf[24];
            char *comma;
            fe_snprintf(buf, sizeof(buf), "%s", args);
            comma = strchr(buf, ',');
            addr = atoi(buf);
            if (comma) count = atoi(comma + 1);
        }
        if (addr < 0 || count < 0) return fe_err(act, "bad args");

        if (strcmp(act, "read_holding") == 0) {
            char out[96];
            u16 n = 0;
            int i;
            if (addr + count > MODBUS_REGS) return fe_err(act, "addr out of range");
            out[n++] = '[';
            for (i = 0; i < count; i++) {
                if (i) out[n++] = ',';
                n += (u16)fe_snprintf(out + n, sizeof(out) - n, "%u",
                                      self->holding_regs[addr + i]);
            }
            out[n++] = ']'; out[n] = 0;
            return fe_ok(act, out);
        }
        if (strcmp(act, "read_input") == 0) {
            char out[96];
            u16 n = 0;
            int i;
            if (addr + count > MODBUS_REGS) return fe_err(act, "addr out of range");
            out[n++] = '[';
            for (i = 0; i < count; i++) {
                if (i) out[n++] = ',';
                n += (u16)fe_snprintf(out + n, sizeof(out) - n, "%u",
                                      self->input_regs[addr + i]);
            }
            out[n++] = ']'; out[n] = 0;
            return fe_ok(act, out);
        }
        if (strcmp(act, "read_coils") == 0) {
            char out[96];
            u16 n = 0;
            int i;
            if (addr + count > MODBUS_REGS) return fe_err(act, "addr out of range");
            out[n++] = '[';
            for (i = 0; i < count; i++) {
                if (i) out[n++] = ',';
                out[n++] = self->coils[addr + i] ? '1' : '0';
            }
            out[n++] = ']'; out[n] = 0;
            return fe_ok(act, out);
        }
        if (strcmp(act, "read_discrete") == 0) {
            char out[96];
            u16 n = 0;
            int i;
            if (addr + count > MODBUS_REGS) return fe_err(act, "addr out of range");
            out[n++] = '[';
            for (i = 0; i < count; i++) {
                if (i) out[n++] = ',';
                out[n++] = self->discrete_inputs[addr + i] ? '1' : '0';
            }
            out[n++] = ']'; out[n] = 0;
            return fe_ok(act, out);
        }
    }

    if (strcmp(act, "write_holding") == 0) {
        // 参数：addr,value
        char buf[24];
        char *comma;
        int a;
        u16 v;
        if (!args || !args[0]) return fe_err(act, "expect addr,value");
        fe_snprintf(buf, sizeof(buf), "%s", args);
        comma = strchr(buf, ',');
        if (!comma) return fe_err(act, "expect addr,value");
        a = atoi(buf);
        v = (u16)atoi(comma + 1);
        if (a < 0 || a >= MODBUS_REGS) return fe_err(act, "addr out of range");
        self->holding_regs[a] = v;
        return fe_ok(act, "{\"written\":true}");
    }
    if (strcmp(act, "write_coil") == 0) {
        char buf[24];
        char *comma;
        int a;
        u8 v;
        if (!args || !args[0]) return fe_err(act, "expect addr,value");
        fe_snprintf(buf, sizeof(buf), "%s", args);
        comma = strchr(buf, ',');
        if (!comma) return fe_err(act, "expect addr,value");
        a = atoi(buf);
        v = (atoi(comma + 1) != 0);
        if (a < 0 || a >= MODBUS_REGS) return fe_err(act, "addr out of range");
        self->coils[a] = v;
        return fe_ok(act, "{\"written\":true}");
    }
    return fe_err(act, "unsupported command");
}

// 供移植层使用的 RTU 从站服务入口：收到完整请求帧后构造响应帧。
// 完整实现见 TODO（可在 main 轮询中调用）。
void modbus_slave_service(modbus_ability_t *self, const u8 *req, u16 len) {
    // TODO: 解析 RTU 帧（unit id + 功能码 0x03/0x04/0x01/0x02/0x06/0x05），
    //       构造响应并经 fe_port_uart_write 返回。
    (void)self; (void)req; (void)len;
    (void)modbus_crc;
}