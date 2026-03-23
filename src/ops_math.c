#include <stdlib.h>
#include "ops.h"
#include "exec.h"

bool op_add(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = a + b;
    return true;
}

bool op_sub(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = a - b;
    return true;
}

bool op_mul(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = a * b;
    return true;
}

bool op_div(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    if (b == 0) {
        vm_thread_flag(t, "line %d: division by zero", i->source_line);
        return false;
    }
    t->regs->R[dest] = a / b;
    return true;
}

bool op_shl(VMThread *t, const Instruction *i) {
    uint32_t reg;
    if (!exec_arg_reg_r(t, &i->args[0], &reg)) return false;
    t->regs->R[reg] <<= 1;
    return true;
}

bool op_shr(VMThread *t, const Instruction *i) {
    uint32_t reg;
    if (!exec_arg_reg_r(t, &i->args[0], &reg)) return false;
    t->regs->R[reg] >>= 1;
    return true;
}

bool op_neg(VMThread *t, const Instruction *i) {
    uint32_t reg;
    if (!exec_arg_reg_r(t, &i->args[0], &reg)) return false;
    t->regs->R[reg] = -t->regs->R[reg];
    return true;
}

bool op_sav(VMThread *t, const Instruction *i) {
    uint32_t dest; int64_t val;
    if (!exec_arg_reg_r(t,  &i->args[0], &dest)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &val))  return false;
    t->regs->R[dest] = val;
    return true;
}

bool op_mod(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    if (b == 0) {
        vm_thread_flag(t, "line %d: modulo by zero", i->source_line);
        return false;
    }
    t->regs->R[dest] = a % b;
    return true;
}

bool op_abs(VMThread *t, const Instruction *i) {
    int64_t a; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_reg_r(t,  &i->args[1], &dest)) return false;
    t->regs->R[dest] = a < 0 ? -a : a;
    return true;
}

bool op_min(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = a < b ? a : b;
    return true;
}

bool op_max(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = a > b ? a : b;
    return true;
}