#ifndef EXEC_H
#define EXEC_H

#include "vm.h"
#include "parser.h"

/*  Execution engine — MANAGER CORE / CORE ALPHA
    ─────────────────────────────────────────────
    exec_thread() is the main dispatch loop.
    It steps through instructions and fans out to the
    appropriate ops_*.c handler based on the opcode.       */

/* Execute the program on the given thread until halt or error */
VMStatus exec_thread(VMThread *t);

/* Execute a single instruction; returns false on halt/error */
bool exec_step(VMThread *t, const Instruction *instr);

/* Evaluate an Arg to an int64 value (resolves registers) */
bool exec_arg_to_int(VMThread *t, const Arg *a, int64_t *out);

/* Evaluate an Arg to a string (resolves S# or imm_str).
   Returns pointer into register memory — do NOT free. */
bool exec_arg_to_str(VMThread *t, const Arg *a, const char **out);

/* Resolve a register index from an Arg (validates bounds) */
bool exec_arg_reg_r(VMThread *t, const Arg *a, uint32_t *out);
bool exec_arg_reg_s(VMThread *t, const Arg *a, uint32_t *out);
bool exec_arg_reg_c(VMThread *t, const Arg *a, uint32_t *out);
bool exec_arg_reg_l(VMThread *t, const Arg *a, uint32_t *out);
bool exec_arg_reg_d(VMThread *t, const Arg *a, uint32_t *out);
bool exec_arg_reg_t(VMThread *t, const Arg *a, uint32_t *out);

#endif /* EXEC_H */