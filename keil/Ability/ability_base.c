// ability_base.c — BaseAbility 实现（C51/8051 版）
// list_data_names / list_ability_names
#include "fe_ability.h"
#include "fe_port.h"
#include <string.h>

fe_output_t ability_base_dispatch(void *inst, const char *act, const char *args) {
    char names[256];
    (void)inst; (void)args;

    if (strcmp(act, "list_data_names") == 0) {
        fe_list_data_names(fe_global_atom(), names, sizeof(names));
        char out[320];
        fe_snprintf(out, sizeof(out), "{\"names\":[%s]}", names);
        return fe_ok(act, out);
    }
    if (strcmp(act, "list_ability_names") == 0) {
        fe_list_ability_names(fe_global_atom(), names, sizeof(names));
        char out[320];
        fe_snprintf(out, sizeof(out), "{\"names\":[%s]}", names);
        return fe_ok(act, out);
    }
    return fe_err(act, "unsupported command");
}