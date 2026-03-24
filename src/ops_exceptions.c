#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "ops.h"
#include "exec.h"

/*  TRY <function_opcode_or_name> [args...] <dest_S#>
    ───────────────────────────────────────────────────
    Attempts to call a user-defined function.
    On success  → S# = "0"
    On failure  → S# = error message string

    We implement this by:
      1. Setting t->in_try = true and saving a jmp_buf
      2. Executing the callee via op_call_user
      3. If THW fires it longjmps back here; we catch it
         and write the thrown message to the dest register.    */

bool op_try(VMThread *t, const Instruction *i) {
    if (i->argc < 2) {
        vm_thread_flag(t, "line %d: TRY requires at least a function and a string register",
                       i->source_line);
        return false;
    }

    /* Last argument is the destination S# register */
    const Arg *dest_arg = &i->args[i->argc - 1];
    uint32_t sreg;
    if (!exec_arg_reg_s(t, dest_arg, &sreg)) return false;

    /* Identify the function opcode from arg[0] */
    uint64_t func_opcode = 0;
    if (i->args[0].type == ARG_OPCODE) {
        func_opcode = i->args[0].u.opcode;
    } else if (i->args[0].type == ARG_IMM_INT) {
        func_opcode = (uint64_t)i->args[0].u.imm_int;
    } else {
        vm_thread_flag(t, "line %d: TRY first arg must be a function opcode",
                       i->source_line);
        return false;
    }

    /* Find the function */
    FuncEntry *fe = vm_find_func(t->vm, func_opcode);
    if (!fe) {
        vm_thread_flag(t, "line %d: TRY: unknown function opcode 0x%016llx",
                       i->source_line, (unsigned long long)func_opcode);
        return false;
    }

    /* Build a synthetic Instruction to pass the call args (args[1]..args[argc-2]) */
    Instruction call_instr = {
        .opcode      = func_opcode,
        .source_line = i->source_line,
        .argc        = i->argc - 2,   /* strip function opcode and dest S# */
        .guarded     = NULL
    };
    for (int a = 0; a < call_instr.argc; a++)
        call_instr.args[a] = i->args[a + 1];

    /* Save prior try state so TRY blocks can be nested */
    bool      prev_in_try    = t->in_try;
    jmp_buf   prev_try_env;
    char     *prev_thrown    = t->thrown_msg;
    t->thrown_msg = NULL;

    if (prev_in_try) memcpy(prev_try_env, t->try_env, sizeof(jmp_buf));

    t->in_try = true;

    if (setjmp(t->try_env) == 0) {
        /* Normal path — execute the function */
        op_call_user(t, func_opcode, &call_instr);
        /* Success */
        regfile_set_string(t->regs, sreg, "0");
        /* Restore prior try state */
        t->status    = VM_OK;   /* clear any flag set inside the call */
        t->in_try    = prev_in_try;
        free(t->thrown_msg);
        t->thrown_msg = prev_thrown;
        if (prev_in_try) memcpy(t->try_env, prev_try_env, sizeof(jmp_buf));
    } else {
        /* Caught a THW */
        const char *msg = t->thrown_msg ? t->thrown_msg : "unknown exception";
        regfile_set_string(t->regs, sreg, msg);
        free(t->thrown_msg);
        t->thrown_msg = prev_thrown;
        t->status     = VM_OK;   /* exception was caught; resume execution */
        t->in_try     = prev_in_try;
        if (prev_in_try) memcpy(t->try_env, prev_try_env, sizeof(jmp_buf));
    }

    return true;
}

/*  THW <S#>
    ─────────
    Throws an exception.  If inside a TRY block, unwinds to it.
    Otherwise crashes the program (unhandled exception).        */
bool op_thw(VMThread *t, const Instruction *i) {
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    const char *msg = regfile_get_string(t->regs, sreg);
    vm_thread_throw(t, msg);
    return false;   /* unreachable unless in_try, but satisfies compiler */
}