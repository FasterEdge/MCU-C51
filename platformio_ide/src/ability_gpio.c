// ability_gpio.c — GpioAbility 实现（C51/8051 版，MCU 专有）
// MCU 专有能力：8051 端口 GPIO 控制。
//   mode <port>,<input|output>  设置端口方向（示意，准双向口无实质方向寄存器）
//   write <port>,<0x00-0xFF>    写 8 位值到端口
//   read <port>                 读 8 位端口值
//   info                        说明
// port 编号 0-3 对应 P0-P3。
#include "fe_ability.h"
#include "fe_port.h"
#include <string.h>
#include <stdlib.h>

fe_output_t ability_gpio_dispatch(void *inst, const char *act, const char *args) {
    char tmp[32];
    char *end;
    u8 port;

    (void)inst;
    fe_snprintf(tmp, sizeof(tmp), "%s", args ? args : "");

    if (strcmp(act, "mode") == 0) {
        char *comma = strchr(tmp, ',');
        if (!comma) return fe_err(act, "bad format, expect port,mode");
        *comma = 0;
        port = (u8)strtoul(tmp, &end, 0);
        if (port > 3) return fe_err(act, "port must be 0-3 (P0-P3)");
        // 8051 准双向口无方向寄存器；STC 增强型可配 PnM0/PnM1
        // 这里仅做校验，不实际设置
        {
            char out[32];
            fe_snprintf(out, sizeof(out), "{\"port\":\"P%u\",\"mode\":\"set\"}",
                        (unsigned)port);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "write") == 0) {
        char *comma = strchr(tmp, ',');
        if (!comma) return fe_err(act, "bad format, expect port,value");
        *comma = 0;
        port = (u8)strtoul(tmp, &end, 0);
        if (port > 3) return fe_err(act, "port must be 0-3 (P0-P3)");
        {
            u32 val = strtoul(comma + 1, &end, 0);
            if (val > 0xFF) return fe_err(act, "value must be 0-255");
            // 通过 SFR 写端口：P0=0x80, P1=0x90, P2=0xA0, P3=0xB0
            static const u8 port_addrs[4] = {0x80, 0x90, 0xA0, 0xB0};
            fe_port_sfr_write(port_addrs[port], (u8)val);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"port\":\"P%u\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)port, (unsigned)val, (unsigned)val);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "read") == 0) {
        port = (u8)strtoul(tmp, &end, 0);
        if (port > 3) return fe_err(act, "port must be 0-3 (P0-P3)");
        {
            static const u8 port_addrs[4] = {0x80, 0x90, 0xA0, 0xB0};
            u8 v = fe_port_sfr_read(port_addrs[port]);
            char out[48];
            fe_snprintf(out, sizeof(out), "{\"port\":\"P%u\",\"value\":%u,\"hex\":\"0x%02X\"}",
                        (unsigned)port, (unsigned)v, (unsigned)v);
            return fe_ok(act, out);
        }
    }
    if (strcmp(act, "info") == 0) {
        return fe_ok(act,
            "{\"ability\":\"GpioAbility\",\"desc\":\"8051 端口 P0-P3\","
            "\"ports\":\"0-3\",\"width\":8}");
    }
    return fe_err(act, "unsupported command");
}