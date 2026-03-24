#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ops.h"
#include "exec.h"

/* ── Helper: build string key from arg ──────────────────────
   Dictionary keys are always stored as strings.
   If the arg is an R# register we convert its integer value.  */
static bool arg_to_key(VMThread *t, const Arg *a, char *buf, size_t bufsz) {
    switch (a->type) {
        case ARG_REG_R: {
            int64_t v;
            if (!exec_arg_to_int(t, a, &v)) return false;
            snprintf(buf, bufsz, "%lld", (long long)v);
            return true;
        }
        case ARG_REG_S: {
            const char *s;
            if (!exec_arg_to_str(t, a, &s)) return false;
            snprintf(buf, bufsz, "%s", s ? s : "");
            return true;
        }
        case ARG_IMM_INT:
            snprintf(buf, bufsz, "%lld", (long long)a->u.imm_int);
            return true;
        case ARG_IMM_STR:
            snprintf(buf, bufsz, "%s", a->u.imm_str ? a->u.imm_str : "");
            return true;
        default:
            vm_thread_flag(t, "SAVD/GET: unsupported key arg type %d", a->type);
            return false;
    }
}

/* ── Helper: build a Value from an arg ───────────────────── */
static bool arg_to_value(VMThread *t, const Arg *a, Value *out) {
    switch (a->type) {
        case ARG_REG_R: {
            int64_t v;
            if (!exec_arg_to_int(t, a, &v)) return false;
            *out = value_int(v);
            return true;
        }
        case ARG_REG_S: {
            const char *s;
            if (!exec_arg_to_str(t, a, &s)) return false;
            *out = value_str(s);
            return true;
        }
        case ARG_REG_L: {
            if (a->u.reg_idx >= REG_L_COUNT) {
                vm_thread_flag(t, "L%u out of bounds", a->u.reg_idx);
                return false;
            }
            *out = (Value){ .type = VAL_LIST_REF, .v.list_idx = a->u.reg_idx };
            return true;
        }
        case ARG_REG_D: {
            if (a->u.reg_idx >= REG_D_COUNT) {
                vm_thread_flag(t, "D%u out of bounds", a->u.reg_idx);
                return false;
            }
            *out = (Value){ .type = VAL_DICT_REF, .v.dict_idx = a->u.reg_idx };
            return true;
        }
        case ARG_IMM_INT:
            *out = value_int(a->u.imm_int);
            return true;
        case ARG_IMM_STR:
            *out = value_str(a->u.imm_str);
            return true;
        default:
            vm_thread_flag(t, "SAVL: unsupported value arg type %d", a->type);
            return false;
    }
}

/* ── SAVL ─────────────────────────────────────────────────── */
bool op_savl(VMThread *t, const Instruction *i) {
    uint32_t lreg;
    if (!exec_arg_reg_l(t, &i->args[0], &lreg)) return false;

    Value val;
    if (!arg_to_value(t, &i->args[1], &val)) return false;

    if (!regfile_list_append(t->regs, lreg, val)) {
        value_free(&val);
        vm_thread_flag(t, "line %d: SAVL failed (list L%u full or alloc error)",
                       i->source_line, lreg);
        return false;
    }
    value_free(&val);
    return true;
}

/* ── SAVD ─────────────────────────────────────────────────── */
bool op_savd(VMThread *t, const Instruction *i) {
    uint32_t dreg;
    if (!exec_arg_reg_d(t, &i->args[0], &dreg)) return false;

    char key[512];
    if (!arg_to_key(t, &i->args[1], key, sizeof(key))) return false;

    Value val;
    if (!arg_to_value(t, &i->args[2], &val)) return false;

    bool ok = regfile_dict_set(t->regs, dreg, key, val);
    value_free(&val);
    if (!ok) {
        vm_thread_flag(t, "line %d: SAVD failed (D%u full or alloc error)",
                       i->source_line, dreg);
        return false;
    }
    return true;
}

/* ── GET ──────────────────────────────────────────────────── */
/* GET <dict> <key>        → stores result into R0 (int) or S0 (str)
   GET <list>[<index>]     → stores result into R0 / S0
   The spec is somewhat loose about the destination — we write
   the result into R0 for integers and S0 for strings unless a
   destination register is provided as the third arg.             */
bool op_get(VMThread *t, const Instruction *i) {
    const Arg *src = &i->args[0];

    /* ── List index form: L0[3] ─────────────────────────── */
    if (src->type == ARG_REG_L_IDX) {
        uint32_t lreg  = src->u.list_idx.reg;
        uint32_t elidx = src->u.list_idx.elem_idx;
        if (lreg >= REG_L_COUNT) {
            vm_thread_flag(t, "line %d: L%u out of bounds", i->source_line, lreg);
            return false;
        }
        Value out;
        if (!regfile_list_get(t->regs, lreg, elidx, &out)) {
            vm_thread_flag(t, "line %d: GET L%u[%u] index out of range",
                           i->source_line, lreg, elidx);
            return false;
        }
        /* Write to explicit dest if given, else R0/S0 */
        if (i->argc >= 2) {
            if (i->args[1].type == ARG_REG_R && out.type == VAL_INT) {
                uint32_t dreg;
                if (!exec_arg_reg_r(t, &i->args[1], &dreg)) { value_free(&out); return false; }
                t->regs->R[dreg] = out.v.i;
            } else if (i->args[1].type == ARG_REG_S && out.type == VAL_STR) {
                uint32_t sreg;
                if (!exec_arg_reg_s(t, &i->args[1], &sreg)) { value_free(&out); return false; }
                regfile_set_string(t->regs, sreg, out.v.s);
            }
        } else {
            if (out.type == VAL_INT)  t->regs->R[0] = out.v.i;
            else if (out.type == VAL_STR) regfile_set_string(t->regs, 0, out.v.s);
        }
        value_free(&out);
        return true;
    }

    /* ── Dictionary form: D0 <key> ──────────────────────── */
    if (src->type == ARG_REG_D) {
        uint32_t dreg;
        if (!exec_arg_reg_d(t, src, &dreg)) return false;

        char key[512];
        if (!arg_to_key(t, &i->args[1], key, sizeof(key))) return false;

        Value out;
        if (!regfile_dict_get(t->regs, dreg, key, &out)) {
            vm_thread_flag(t, "line %d: GET D%u key '%s' not found",
                           i->source_line, dreg, key);
            return false;
        }
        if (i->argc >= 3) {
            if (i->args[2].type == ARG_REG_R && out.type == VAL_INT) {
                uint32_t dest;
                if (!exec_arg_reg_r(t, &i->args[2], &dest)) { value_free(&out); return false; }
                t->regs->R[dest] = out.v.i;
            } else if (i->args[2].type == ARG_REG_S && out.type == VAL_STR) {
                uint32_t dest;
                if (!exec_arg_reg_s(t, &i->args[2], &dest)) { value_free(&out); return false; }
                regfile_set_string(t->regs, dest, out.v.s);
            }
        } else {
            if (out.type == VAL_INT)  t->regs->R[0] = out.v.i;
            else if (out.type == VAL_STR) regfile_set_string(t->regs, 0, out.v.s);
        }
        value_free(&out);
        return true;
    }

    vm_thread_flag(t, "line %d: GET expects L#[i] or D# as first arg", i->source_line);
    return false;
}

/* ── LEN ──────────────────────────────────────────────────── */
bool op_len(VMThread *t, const Instruction *i) {
    uint32_t dreg;
    if (!exec_arg_reg_r(t, &i->args[0], &dreg)) return false;

    const Arg *col = &i->args[1];
    size_t count = 0;

    if (col->type == ARG_REG_L) {
        if (col->u.reg_idx >= REG_L_COUNT) {
            vm_thread_flag(t, "L%u out of bounds", col->u.reg_idx);
            return false;
        }
        count = regfile_list_len(t->regs, col->u.reg_idx);
    } else if (col->type == ARG_REG_D) {
        if (col->u.reg_idx >= REG_D_COUNT) {
            vm_thread_flag(t, "D%u out of bounds", col->u.reg_idx);
            return false;
        }
        count = regfile_dict_len(t->regs, col->u.reg_idx);
    } else {
        vm_thread_flag(t, "line %d: LEN requires L# or D# register", i->source_line);
        return false;
    }

    t->regs->R[dreg] = (int64_t)count;
    return true;
}

/* ── CLRL ─────────────────────────────────────────────────── */
bool op_clrl(VMThread *t, const Instruction *i) {
    uint32_t lreg;
    if (!exec_arg_reg_l(t, &i->args[0], &lreg)) return false;
    regfile_list_clear(t->regs, lreg);
    return true;
}

/* ── DELD ─────────────────────────────────────────────────── */
bool op_deld(VMThread *t, const Instruction *i) {
    uint32_t dreg;
    if (!exec_arg_reg_d(t, &i->args[0], &dreg)) return false;

    char key[512];
    if (!arg_to_key(t, &i->args[1], key, sizeof(key))) return false;

    if (!regfile_dict_del(t->regs, dreg, key)) {
        vm_thread_flag(t, "line %d: DELD key '%s' not found in D%u",
                       i->source_line, key, dreg);
        return false;
    }
    return true;
}