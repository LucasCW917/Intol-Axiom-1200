#include "ops.h"
#include "exec.h"

/*  All higher-level bitwise ops are defined in terms of NAND,
    exactly as the spec documents them.  We operate on full
    64-bit integers (bitwise across all bits).                 */

static inline int64_t bw_nand(int64_t a, int64_t b) { return ~(a & b); }
static inline int64_t bw_not (int64_t a)             { return bw_nand(a, a); }
static inline int64_t bw_and (int64_t a, int64_t b)  { return bw_nand(bw_nand(a,b), bw_nand(a,b)); }
static inline int64_t bw_or  (int64_t a, int64_t b)  { return bw_nand(bw_nand(a,a), bw_nand(b,b)); }
static inline int64_t bw_nor (int64_t a, int64_t b)  { return bw_nand(bw_or(a,b), bw_or(a,b)); }
static inline int64_t bw_xor (int64_t a, int64_t b)  {
    int64_t n = bw_nand(a, b);
    return bw_nand(bw_nand(a, n), bw_nand(b, n));
}
static inline int64_t bw_xnor(int64_t a, int64_t b)  { return bw_nand(bw_xor(a,b), bw_xor(a,b)); }
static inline int64_t bw_buf (int64_t a)              { return bw_nand(bw_nand(a,a), bw_nand(a,a)); }

bool op_nand(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_nand(a, b);
    return true;
}

bool op_not(VMThread *t, const Instruction *i) {
    int64_t a; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_reg_r(t,  &i->args[1], &dest)) return false;
    t->regs->R[dest] = bw_not(a);
    return true;
}

bool op_and(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_and(a, b);
    return true;
}

bool op_or(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_or(a, b);
    return true;
}

bool op_nor(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_nor(a, b);
    return true;
}

bool op_xor(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_xor(a, b);
    return true;
}

bool op_xnor(VMThread *t, const Instruction *i) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;
    t->regs->R[dest] = bw_xnor(a, b);
    return true;
}

bool op_buf(VMThread *t, const Instruction *i) {
    int64_t a; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_reg_r(t,  &i->args[1], &dest)) return false;
    t->regs->R[dest] = bw_buf(a);
    return true;
}