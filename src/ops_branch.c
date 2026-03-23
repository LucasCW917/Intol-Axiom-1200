#include <string.h>
#include "ops.h"
#include "exec.h"

/* STP is a no-op at runtime; labels are resolved at parse time */
bool op_stp(VMThread *t, const Instruction *i) {
    (void)t; (void)i;
    return true;
}

bool op_jmp(VMThread *t, const Instruction *i) {
    if (i->argc < 1 || i->args[0].type != ARG_LABEL) {
        vm_thread_flag(t, "line %d: JMP requires a label argument", i->source_line);
        return false;
    }

    /* Resolve label in the current program context */
    Program *prog = (t->call_depth > 0)
        ? t->call_stack[t->call_depth - 1].program
        : t->program;

    size_t pos;
    if (!program_find_label(prog, i->args[0].u.label, &pos)) {
        vm_thread_flag(t, "line %d: undefined label '%s'",
                       i->source_line, i->args[0].u.label);
        return false;
    }

    t->ip = pos;
    return true;
}