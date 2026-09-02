// FasterEdge 开源项目 - Github: https://github.com/FasterEdge - Gitee: https://gitee.com/FasterEdge
// data_chip.c — ChipData 实现（C51/8051 版，MCU 专有）
// MCU 专有 Data：芯片信息（平台差异由 fe_port 提供）。
//   info   返回芯片型号 / 内核 / 频率 / RAM / Flash 等
#include "fe_data.h"
#include "fe_port.h"
#include <string.h>

fe_output_t data_chip_dispatch(void *inst, const char *act, const char *args) {
    (void)inst; (void)args;
    if (strcmp(act, "info") == 0) {
        char out[160];
        fe_port_chip_info(out, sizeof(out));
        return fe_ok(act, out);
    }
    return fe_err(act, "unsupported command");
}
