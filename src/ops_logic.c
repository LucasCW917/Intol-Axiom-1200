#include "ops.h"
#include "exec.h"

bool op_ifeq(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = (a == b) ? 1 : 0;
    return true;
}

bool op_ifnq(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = (a != b) ? 1 : 0;
    return true;
}

bool op_iftr(VMThread *t, const Instruction *i) {
    int64_t cond;
    if (!exec_arg_to_int(t, &i->args[0], &cond)) return false;
    if (cond == 1 && i->guarded) return exec_step(t, (const Instruction *)i->guarded);
    return true;
}

bool op_iffl(VMThread *t, const Instruction *i) {
    int64_t cond;
    if (!exec_arg_to_int(t, &i->args[0], &cond)) return false;
    if (cond == 0 && i->guarded) return exec_step(t, (const Instruction *)i->guarded);
    return true;
}

bool op_whle(VMThread *t, const Instruction *i) {
    if (!i->guarded) return true;
    int64_t cond;
    while (true) {
        if (!exec_arg_to_int(t, &i->args[0], &cond)) return false;
        if (cond != 1) break;
        if (!exec_step(t, (const Instruction *)i->guarded)) return false;
        if (t->status != VM_OK) return false;
    }
    return true;
}