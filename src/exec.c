#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exec.h"
#include "ops.h"
#include "opcodes.h"

/* ── Argument resolution ──────────────────────────────────── */

bool exec_arg_to_int(VMThread *t, const Arg *a, int64_t *out) {
    switch (a->type) {
        case ARG_REG_R:
            if (a->u.reg_idx >= REG_R_COUNT) {
                vm_thread_flag(t, "R register index %u out of bounds", a->u.reg_idx);
                return false;
            }
            *out = t->regs->R[a->u.reg_idx];
            return true;
        case ARG_REG_C:
            if (a->u.reg_idx >= REG_C_COUNT) {
                vm_thread_flag(t, "C register index %u out of bounds", a->u.reg_idx);
                return false;
            }
            *out = (int64_t)(unsigned char)t->regs->C[a->u.reg_idx];
            return true;
        case ARG_IMM_INT:
            *out = a->u.imm_int;
            return true;
        default:
            vm_thread_flag(t, "Cannot convert arg type %d to int", a->type);
            return false;
    }
}

bool exec_arg_to_str(VMThread *t, const Arg *a, const char **out) {
    if (a->type == ARG_REG_S) {
        if (a->u.reg_idx >= REG_S_COUNT) {
            vm_thread_flag(t, "S register index %u out of bounds", a->u.reg_idx);
            return false;
        }
        *out = regfile_get_string(t->regs, a->u.reg_idx);
        return true;
    }
    if (a->type == ARG_IMM_STR) {
        *out = a->u.imm_str ? a->u.imm_str : "";
        return true;
    }
    vm_thread_flag(t, "Cannot convert arg type %d to string", a->type);
    return false;
}

bool exec_arg_reg_r(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_R) {
        vm_thread_flag(t, "Expected R register, got type %d", a->type);
        return false;
    }
    if (a->u.reg_idx >= REG_R_COUNT) {
        vm_thread_flag(t, "R%u out of bounds", a->u.reg_idx);
        return false;
    }
    *out = a->u.reg_idx;
    return true;
}

bool exec_arg_reg_s(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_S) {
        vm_thread_flag(t, "Expected S register, got type %d", a->type);
        return false;
    }
    if (a->u.reg_idx >= REG_S_COUNT) {
        vm_thread_flag(t, "S%u out of bounds", a->u.reg_idx);
        return false;
    }
    *out = a->u.reg_idx;
    return true;
}

bool exec_arg_reg_c(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_C) {
        vm_thread_flag(t, "Expected C register, got type %d", a->type);
        return false;
    }
    if (a->u.reg_idx >= REG_C_COUNT) {
        vm_thread_flag(t, "C%u out of bounds", a->u.reg_idx);
        return false;
    }
    *out = a->u.reg_idx;
    return true;
}

bool exec_arg_reg_l(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_L && a->type != ARG_REG_L_IDX) {
        vm_thread_flag(t, "Expected L register, got type %d", a->type);
        return false;
    }
    uint32_t idx = (a->type == ARG_REG_L_IDX) ? a->u.list_idx.reg : a->u.reg_idx;
    if (idx >= REG_L_COUNT) {
        vm_thread_flag(t, "L%u out of bounds", idx);
        return false;
    }
    *out = idx;
    return true;
}

bool exec_arg_reg_d(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_D) {
        vm_thread_flag(t, "Expected D register, got type %d", a->type);
        return false;
    }
    if (a->u.reg_idx >= REG_D_COUNT) {
        vm_thread_flag(t, "D%u out of bounds", a->u.reg_idx);
        return false;
    }
    *out = a->u.reg_idx;
    return true;
}

bool exec_arg_reg_t(VMThread *t, const Arg *a, uint32_t *out) {
    if (a->type != ARG_REG_T) {
        vm_thread_flag(t, "Expected T register, got type %d", a->type);
        return false;
    }
    *out = a->u.reg_idx;
    return true;
}

/* ── Dispatch table ───────────────────────────────────────── */

typedef bool (*OpHandler)(VMThread *, const Instruction *);

typedef struct {
    uint64_t   opcode;
    OpHandler  handler;
} DispatchEntry;

static const DispatchEntry DISPATCH[] = {
    /* Math */
    {OP_ADD,   op_add},  {OP_SUB,   op_sub},  {OP_MUL,   op_mul},
    {OP_DIV,   op_div},  {OP_SHL,   op_shl},  {OP_SHR,   op_shr},
    {OP_NEG,   op_neg},  {OP_SAV,   op_sav},
    /* QoL math */
    {OP_MOD,   op_mod},  {OP_ABS,   op_abs},  {OP_MIN,   op_min},
    {OP_MAX,   op_max},
    /* Logic */
    {OP_IFEQ,  op_ifeq}, {OP_IFNQ,  op_ifnq}, {OP_IFTR,  op_iftr},
    {OP_IFFL,  op_iffl}, {OP_WHLE,  op_whle},
    /* Functions */
    {OP_DEF,   op_def},  {OP_RET,   op_ret},
    /* Branch */
    {OP_STP,   op_stp},  {OP_JMP,   op_jmp},
    /* I/O */
    {OP_OUT,   op_out},  {OP_INP,   op_inp},  {OP_DSTR,  op_dstr},
    {OP_TCHA,  op_tcha}, {OP_TINT,  op_tint}, {OP_UGST,  op_ugst},
    {OP_GSTR,  op_gstr}, {OP_SAPP,  op_sapp}, {OP_ITOS,  op_itos},
    {OP_STOI,  op_stoi}, {OP_WRAW,  op_wraw}, {OP_SLEN,  op_slen},
    {OP_SCMP,  op_scmp}, {OP_ISNUM, op_isnum},
    /* Collections */
    {OP_SAVL,  op_savl}, {OP_SAVD,  op_savd}, {OP_GET,   op_get},
    {OP_LEN,   op_len},  {OP_CLRL,  op_clrl}, {OP_DELD,  op_deld},
    /* Bitwise */
    {OP_NAND,  op_nand}, {OP_NOT,   op_not},  {OP_AND,   op_and},
    {OP_OR,    op_or},   {OP_NOR,   op_nor},  {OP_XOR,   op_xor},
    {OP_XNOR,  op_xnor}, {OP_BUF,   op_buf},
    /* Exceptions */
    {OP_TRY,   op_try},  {OP_THW,   op_thw},
    /* Time */
    {OP_GTM,   op_gtm},  {OP_CTM,   op_ctm},  {OP_STAL,  op_stal},
    /* Concurrency */
    {OP_FORK,  op_fork}, {OP_JOIN,  op_join}, {OP_LOCK,  op_lock},
    {OP_UNLO,  op_unlo}, {OP_AADD,  op_aadd}, {OP_ASUB,  op_asub},
    {OP_AMUL,  op_amul}, {OP_ADIV,  op_adiv}, {OP_YIEL,  op_yiel},
    {OP_THID,  op_thid}, {OP_THST,  op_thst},
    /* Debug */
    {OP_DBG,   op_dbg},
    {0, NULL}
};

static OpHandler dispatch_lookup(uint64_t opcode) {
    for (int i = 0; DISPATCH[i].handler; i++) {
        if (DISPATCH[i].opcode == opcode) return DISPATCH[i].handler;
    }
    return NULL;
}

/* ── Single instruction step ──────────────────────────────── */

bool exec_step(VMThread *t, const Instruction *instr) {
    if (t->status == VM_CRASH || t->status == VM_HALTED) return false;

    OpHandler h = dispatch_lookup(instr->opcode);
    if (h) return h(t, instr);

    /* Not a built-in — check user-defined functions */
    if (!ns_is_builtin(instr->opcode)) {
        return op_call_user(t, instr->opcode, instr);
    }

    vm_thread_flag(t, "line %d: unknown opcode 0x%016llx",
                   instr->source_line, (unsigned long long)instr->opcode);
    return false;
}

/* ── Main execution loop ──────────────────────────────────── */

VMStatus exec_thread(VMThread *t) {
    while (t->running && t->status == VM_OK) {
        Program *prog = (t->call_depth > 0)
            ? t->call_stack[t->call_depth - 1].program
            : t->program;

        if (t->ip >= prog->count) {
            /* End of program/function */
            if (t->call_depth > 0) {
                /* Return from function with no explicit RET */
                t->call_depth--;
                CallFrame *frame = &t->call_stack[t->call_depth];
                t->ip      = frame->ret_ip;
                t->program = frame->ret_program;
            } else {
                t->status  = VM_HALTED;
                t->running = false;
                break;
            }
            continue;
        }

        const Instruction *instr = &prog->instrs[t->ip];
        t->ip++;

        if (!exec_step(t, instr)) {
            if (t->status == VM_OK) t->status = VM_FLAGGED;
            break;
        }
    }
    return t->status;
}