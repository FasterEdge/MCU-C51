// main.c — FasterEdge MCU C51/8051 入口
// 串口命令解释器：输入 "data_xxx act args" / "ability_xxx act args"
// 示例：
//   ability_BaseAbility list_ability_names
//   data_ConfigData set wifi.ssid=MyNet
//   ability_TimeAbility sync_manual 1700000000
//   ability_ModbusAbility write_holding 0,42
#include "fe.h"
#include "fe_ability.h"
#include "fe_data.h"
#include "fe_port.h"
#include <string.h>

// 命令行缓冲
static char line[96];
static u16  line_len = 0;

// 从串口读取一行（轮询）
static u8 read_line(char *buf, u16 buflen, u16 *len) {
    while (fe_port_uart_available(0)) {
        int c = fe_port_uart_read(0);
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (line_len == 0) continue;
            memcpy(buf, line, line_len);
            buf[line_len] = 0;
            *len = line_len;
            line_len = 0;
            return TRUE;
        }
        if (line_len + 1 < sizeof(line)) line[line_len++] = (char)c;
    }
    return FALSE;
}

static void print_help(void) {
    char names[160];
    fe_port_uart_write(0, (const u8 *)"Usage: <data|ability>_<Name> <act> [args]\r\n", 45);
    fe_list_ability_names(fe_global_atom(), names, sizeof(names));
    fe_port_uart_write(0, (const u8 *)"abilities: ", 11);
    fe_port_uart_write(0, (const u8 *)names, (u16)strlen(names));
    fe_port_uart_write(0, (const u8 *)"\r\n", 2);
    fe_list_data_names(fe_global_atom(), names, sizeof(names));
    fe_port_uart_write(0, (const u8 *)"data: ", 6);
    fe_port_uart_write(0, (const u8 *)names, (u16)strlen(names));
    fe_port_uart_write(0, (const u8 *)"\r\nexamples:\r\n", 13);
    fe_port_uart_write(0, (const u8 *)"  ability_BaseAbility list_ability_names\r\n", 42);
    fe_port_uart_write(0, (const u8 *)"  ability_TimeAbility sync_manual 1700000000\r\n", 46);
    fe_port_uart_write(0, (const u8 *)"  ability_ModbusAbility write_holding 0,42\r\n", 45);
    fe_port_uart_write(0, (const u8 *)"  data_ConfigData set wifi.ssid=MyNet\r\n", 39);
}

void main(void) {
    const char *banner = "\r\nFasterEdge-MCU (8051 / Keil C51)\r\ninput: <data|ability>_<Name> <act> [args]  |  'help'\r\n";

    fe_port_uart_init(0, 115200, NULL, NULL);
    fe_port_uart_write(0, (const u8 *)banner, (u16)strlen(banner));

    fe_init_all();

    for (;;) {
        char buf[96];
        u16 len;
        if (read_line(buf, sizeof(buf), &len)) {
            char *sp1, *target, *rest, *sp2, *act, *args;
            fe_output_t out;
            char oline[128];
            while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = 0;

            if (strcmp(buf, "help") == 0) { print_help(); continue; }

            sp1 = strchr(buf, ' ');
            if (!sp1) {
                fe_port_uart_write(0, (const u8 *)"bad command\r\n", 13);
                continue;
            }
            *sp1 = 0;
            target = buf;
            rest = sp1 + 1;
            sp2 = strchr(rest, ' ');
            if (sp2) { *sp2 = 0; act = rest; args = sp2 + 1; }
            else     { act = rest; args = NULL; }

            out = fe_execute(fe_global_atom(), target, act, args ? args : "");
            if (out.ok)
                fe_snprintf(oline, sizeof(oline), "OK %s -> %s\r\n", out.name, out.value);
            else
                fe_snprintf(oline, sizeof(oline), "ERR %s: %s\r\n", out.name, out.err);
            fe_port_uart_write(0, (const u8 *)oline, (u16)strlen(oline));
        }
        fe_port_delay_ms(10);
    }
}