#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ops.h"
#include "exec.h"

/* ── DEF ──────────────────────────────────────────────────── */
/*  DEF <opcode> <name> <file.apo> [args...]
    Registers a function in the VM's function table.
    The function body is compiled lazily on first call.         */
bool op_def(VMThread *t, const Instruction *i) {
    if (i->argc < 3) {
        vm_thread_flag(t, "line %d: DEF requires opcode, name, file", i->source_line);
        return false;
    }

    /* arg[0]: opcode — either a hex literal or ARG_OPCODE */
    uint64_t func_opcode = 0;
    if (i->args[0].type == ARG_OPCODE) {
        func_opcode = i->args[0].u.opcode;
    } else if (i->args[0].type == ARG_IMM_INT) {
        func_opcode = (uint64_t)i->args[0].u.imm_int;
    } else {
        vm_thread_flag(t, "line %d: DEF: first arg must be an opcode value", i->source_line);
        return false;
    }

    /* arg[1]: function name */
    const char *name = NULL;
    if (i->args[1].type == ARG_LABEL)   name = i->args[1].u.label;
    else if (i->args[1].type == ARG_IMM_STR) name = i->args[1].u.imm_str;
    if (!name) {
        vm_thread_flag(t, "line %d: DEF: second arg must be a name", i->source_line);
        return false;
    }

    /* arg[2]: file path */
    const char *file = NULL;
    if (i->args[2].type == ARG_LABEL)   file = i->args[2].u.label;
    else if (i->args[2].type == ARG_IMM_STR) file = i->args[2].u.imm_str;
    if (!file) {
        vm_thread_flag(t, "line %d: DEF: third arg must be a file path", i->source_line);
        return false;
    }

    int argc = i->argc - 3;   /* remaining args are parameter registers */
    if (!vm_register_func(t->vm, func_opcode, name, file, argc)) {
        /* Already registered — not a hard error, just a warning */
        fprintf(stderr, "[WARN] DEF: function opcode 0x%016llx already registered\n",
                (unsigned long long)func_opcode);
    }
    return true;
}

/* ── TAKE ─────────────────────────────────────────────────── */
/*  TAKE <R0> [R1 R2 ...]
    Reads from the current call frame's argument list and stores
    into the specified destination registers.                    */
bool op_take(VMThread *t, const Instruction *i) {
    if (t->call_depth == 0) {
        vm_thread_flag(t, "line %d: TAKE used outside a function", i->source_line);
        return false;
    }
    CallFrame *frame = &t->call_stack[t->call_depth - 1];

    for (int a = 0; a < i->argc; a++) {
        uint32_t dreg;
        if (!exec_arg_reg_r(t, &i->args[a], &dreg)) return false;

        if (a < frame->argc) {
            /* arg was passed — write it in */
            int64_t val;
            if (!exec_arg_to_int(t, &frame->args[a], &val)) return false;
            t->regs->R[dreg] = val;
        }
        /* extra declared params beyond what was passed are left unchanged */
    }
    return true;
}

/* ── RET ──────────────────────────────────────────────────── */
bool op_ret(VMThread *t, const Instruction *i) {
    if (t->call_depth == 0) {
        vm_thread_flag(t, "line %d: RET used outside a function", i->source_line);
        return false;
    }

    /* Capture return value before we pop the frame */
    int64_t ret_val = 0;
    bool    has_ret = (i->argc > 0);
    if (has_ret) {
        if (!exec_arg_to_int(t, &i->args[0], &ret_val)) return false;
    }

    /* Pop call frame */
    t->call_depth--;
    CallFrame *frame = &t->call_stack[t->call_depth];

    /* Restore IP and program context */
    t->ip      = frame->ret_ip;

    /* Store return value into R0 of the caller's context */
    if (has_ret) t->regs->R[0] = ret_val;

    /* Free any heap-allocated args in the frame */
    for (int a = 0; a < frame->argc; a++) arg_free(&frame->args[a]);

    return true;
}

/* ── User-defined function call ───────────────────────────── */
/*  Called by exec_step when the opcode is not a built-in.
    Looks up the function, lazily compiles it, pushes a call
    frame, and jumps into the function body.                   */
bool op_call_user(VMThread *t, uint64_t opcode, const Instruction *i) {
    FuncEntry *fe = vm_find_func(t->vm, opcode);
    if (!fe) {
        vm_thread_flag(t, "line %d: call to unknown function opcode 0x%016llx",
                       i->source_line, (unsigned long long)opcode);
        return false;
    }

    /* Lazy compilation */
    if (!fe->program) {
        fe->program = program_parse_file(fe->file_path);
        if (!fe->program) {
            vm_thread_flag(t, "line %d: failed to compile function '%s' from '%s'",
                           i->source_line, fe->name, fe->file_path);
            return false;
        }
    }

    /* Grow call stack if needed */
    if (t->call_depth >= t->call_capacity) {
        int newcap = t->call_capacity * 2;
        CallFrame *newstack = realloc(t->call_stack, newcap * sizeof(CallFrame));
        if (!newstack) {
            vm_thread_flag(t, "call stack overflow");
            return false;
        }
        t->call_stack    = newstack;
        t->call_capacity = newcap;
    }

    /* Build call frame — save return context and copy args */
    CallFrame *frame   = &t->call_stack[t->call_depth];
    frame->ret_ip      = t->ip;          /* IP already advanced past this instr */
    frame->ret_program = t->program;
    frame->argc        = i->argc;

    /* Clone args so they survive for TAKE */
    for (int a = 0; a < i->argc && a < MAX_ARGS; a++)
        frame->args[a] = arg_clone(&i->args[a]);

    t->call_depth++;

    /* Jump into function */
    t->ip      = 0;
    t->program = fe->program;

    return true;
}