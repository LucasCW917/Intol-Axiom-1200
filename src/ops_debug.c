#include <stdio.h>
#include "ops.h"
#include "exec.h"

bool op_dbg(VMThread *t, const Instruction *i) {
    if (i->argc < 1) return true;
    const Arg *a = &i->args[0];

    switch (a->type) {
        case ARG_REG_R:
            if (a->u.reg_idx < REG_R_COUNT)
                fprintf(stderr, "[DBG] R%u = %lld\n",
                        a->u.reg_idx, (long long)t->regs->R[a->u.reg_idx]);
            break;
        case ARG_REG_C:
            if (a->u.reg_idx < REG_C_COUNT)
                fprintf(stderr, "[DBG] C%u = '%c' (%d)\n",
                        a->u.reg_idx,
                        t->regs->C[a->u.reg_idx],
                        (int)(unsigned char)t->regs->C[a->u.reg_idx]);
            break;
        case ARG_REG_S: {
            if (a->u.reg_idx < REG_S_COUNT) {
                const char *s = regfile_get_string(t->regs, a->u.reg_idx);
                fprintf(stderr, "[DBG] S%u = \"%s\"\n", a->u.reg_idx, s ? s : "");
            }
            break;
        }
        case ARG_REG_L: {
            uint32_t idx = a->u.reg_idx;
            if (idx >= REG_L_COUNT) break;
            size_t len = regfile_list_len(t->regs, idx);
            fprintf(stderr, "[DBG] L%u = [", idx);
            for (size_t k = 0; k < len; k++) {
                Value v; regfile_list_get(t->regs, idx, k, &v);
                if (v.type == VAL_INT)  fprintf(stderr, "%lld", (long long)v.v.i);
                else if (v.type == VAL_STR) fprintf(stderr, "\"%s\"", v.v.s ? v.v.s : "");
                else fprintf(stderr, "<ref>");
                if (k + 1 < len) fprintf(stderr, ", ");
                value_free(&v);
            }
            fprintf(stderr, "] (len=%zu)\n", len);
            break;
        }
        case ARG_REG_D: {
            uint32_t idx = a->u.reg_idx;
            if (idx >= REG_D_COUNT) break;
            size_t len = regfile_dict_len(t->regs, idx);
            fprintf(stderr, "[DBG] D%u = {%zu entries}\n", idx, len);
            break;
        }
        default:
            fprintf(stderr, "[DBG] (unsupported register type %d)\n", a->type);
            break;
    }
    return true;
}