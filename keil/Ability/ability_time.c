// ability_time.c — TimeAbility 实现（C51/8051 版）
// sync_manual / sync_system / get_time / configure_run
// C51 无网络，不含 sync_ntp/sync_net。时间源由 fe_port 抽象
// （RTC 芯片 / 定时器计数）。epoch 用 u32（到 2106 年够用）。
#include "fe_ability.h"
#include "fe_port.h"
#include <stdlib.h>
#include <string.h>

fe_output_t ability_time_dispatch(void *inst, const char *act, const char *args) {
    time_ability_t *self = (time_ability_t *)inst;

    if (strcmp(act, "get_time") == 0) {
        u32 now = fe_port_time_now();
        char out[48];
        fe_snprintf(out, sizeof(out), "{\"epoch\":%lu}", (unsigned long)now);
        return fe_ok(act, out);
    }
    if (strcmp(act, "sync_manual") == 0) {
        if (!args || args[0] == 0)
            return fe_err(act, "missing epoch");
        u32 ep = (u32)strtoul(args, NULL, 10);
        if (ep == 0) return fe_err(act, "invalid epoch");
        fe_port_time_set(ep);
        self->manual_epoch = ep;
        char out[48];
        fe_snprintf(out, sizeof(out), "epoch=%lu", (unsigned long)ep);
        return fe_ok(act, out);
    }
    if (strcmp(act, "sync_system") == 0) {
        u32 now = fe_port_time_now();
        char out[48];
        fe_snprintf(out, sizeof(out), "epoch=%lu", (unsigned long)now);
        return fe_ok(act, out);
    }
    if (strcmp(act, "configure_run") == 0) {
        // TODO: 配置周期校时，在 main 调度中实现
        return fe_ok(act, "configured");
    }
    return fe_err(act, "unsupported command");
}