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
        // A 16-bit register needs up to 5 digits plus a comma.  Cap textual
        // reads so the fixed 96-byte response buffer cannot overflow.
        if (count > 12) return fe_err(act, "count too large");

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
// req 必须是一个完整的 RTU 帧（地址、功能码、PDU、CRC；CRC 低字节在前）。
// 服务入口不分配内存，响应最大为 69 字节（本实现的 32 项寄存器表）。
static void modbus_send_response(const u8 *pdu, u8 pdu_len) {
    u8 frame[69];
    u16 crc;
    u8 i;
    if ((u16)pdu_len + 2 > (u16)sizeof(frame)) return;
    for (i = 0; i < pdu_len; i++) frame[i] = pdu[i];
    crc = modbus_crc(frame, pdu_len);
    frame[pdu_len] = (u8)(crc & 0xFF);
    frame[(u8)(pdu_len + 1)] = (u8)(crc >> 8);
    (void)fe_port_uart_write(0, frame, (u16)pdu_len + 2);
}

static void modbus_send_exception(u8 unit, u8 function, u8 exception) {
    u8 pdu[3];
    pdu[0] = unit;
    pdu[1] = (u8)(function | 0x80);
    pdu[2] = exception;
    modbus_send_response(pdu, 3);
}

static u8 modbus_range_ok(u16 address, u16 count) {
    return count != 0 && address < MODBUS_REGS && count <= MODBUS_REGS - address;
}

void modbus_slave_service(modbus_ability_t *self, const u8 *req, u16 len) {
    u8 unit, function;
    u16 received_crc, calculated_crc;
    u16 address, count, value;
    u8 pdu[67];
    u8 byte_count, i, out_len;
    u8 broadcast;

    // 最短请求是 8 字节；先做边界和 CRC 检查，坏帧静默丢弃。
    if (!self || !req || len < 8) return;
    unit = req[0];
    function = req[1];
    if (unit != self->unit_id && unit != 0) return;
    received_crc = (u16)req[len - 2] | ((u16)req[len - 1] << 8);
    calculated_crc = modbus_crc(req, (u16)(len - 2));
    if (received_crc != calculated_crc) return;
    broadcast = (unit == 0);
    if (broadcast && (function < 5 || function > 6)) return;

    // 0x01/0x02/0x03/0x04：起始地址和数量均为大端序。
    if (function == 1 || function == 2 || function == 3 || function == 4) {
        if (len != 8) return;
        address = ((u16)req[2] << 8) | req[3];
        count = ((u16)req[4] << 8) | req[5];
        if (!modbus_range_ok(address, count)) {
            if (!broadcast) modbus_send_exception(unit, function, 2);
            return;
        }
        if (function == 1 || function == 2) {
            byte_count = (u8)((count + 7) / 8);
            pdu[0] = unit; pdu[1] = function; pdu[2] = byte_count;
            for (i = 0; i < byte_count; i++) pdu[(u8)(3 + i)] = 0;
            for (i = 0; i < count; i++) {
                u8 bit = (u8)(address + i);
                u8 state = (function == 1) ? self->coils[bit] : self->discrete_inputs[bit];
                if (state) pdu[(u8)(3 + i / 8)] |= (u8)(1 << (i % 8));
            }
            modbus_send_response(pdu, (u8)(byte_count + 3));
        } else {
            // 本地表只有 32 项；因此自然也限制了标准允许的 125 项。
            byte_count = (u8)(count * 2);
            pdu[0] = unit; pdu[1] = function; pdu[2] = byte_count;
            out_len = 3;
            for (i = 0; i < count; i++) {
                value = (function == 3) ? self->holding_regs[address + i] : self->input_regs[address + i];
                pdu[out_len++] = (u8)(value >> 8);
                pdu[out_len++] = (u8)value;
            }
            modbus_send_response(pdu, out_len);
        }
        return;
    }

    // 0x05：写单线圈，只接受 0xFF00 或 0x0000。
    if (function == 5) {
        if (len != 8) return;
        address = ((u16)req[2] << 8) | req[3];
        value = ((u16)req[4] << 8) | req[5];
        if (address >= MODBUS_REGS) { if (!broadcast) modbus_send_exception(unit, function, 2); return; }
        if (value != 0x0000 && value != 0xFF00) { if (!broadcast) modbus_send_exception(unit, function, 3); return; }
        self->coils[address] = (value == 0xFF00) ? 1 : 0;
        if (!broadcast) modbus_send_response(req, 6);
        return;
    }

    // 0x06：写单个保持寄存器；响应为请求 PDU 的回显。
    if (function == 6) {
        if (len != 8) return;
        address = ((u16)req[2] << 8) | req[3];
        if (address >= MODBUS_REGS) { if (!broadcast) modbus_send_exception(unit, function, 2); return; }
        value = ((u16)req[4] << 8) | req[5];
        self->holding_regs[address] = value;
        if (!broadcast) modbus_send_response(req, 6);
        return;
    }
    if (!broadcast) modbus_send_exception(unit, function, 1);
}