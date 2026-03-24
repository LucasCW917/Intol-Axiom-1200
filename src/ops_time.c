#include <time.h>
#include <stdio.h>
#include <string.h>
#include "ops.h"
#include "exec.h"

bool op_gtm(VMThread *t, const Instruction *i) {
    uint32_t dreg;
    if (!exec_arg_reg_r(t, &i->args[0], &dreg)) return false;
    t->regs->R[dreg] = (int64_t)time(NULL);
    return true;
}

bool op_ctm(VMThread *t, const Instruction *i) {
    int64_t ts; uint32_t sreg;
    if (!exec_arg_to_int(t, &i->args[0], &ts)) return false;
    if (!exec_arg_reg_s(t,  &i->args[1], &sreg)) return false;

    time_t raw = (time_t)ts;
    char buf[64];
    /* ctime_r is thread-safe; strip the trailing newline ctime appends */
#ifdef _WIN32
    ctime_s(buf, sizeof(buf), &raw);
#else
    ctime_r(&raw, buf);
#endif
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    regfile_set_string(t->regs, sreg, buf);
    return true;
}

bool op_stal(VMThread *t, const Instruction *i) {
    int64_t secs;
    if (!exec_arg_to_int(t, &i->args[0], &secs)) return false;
    if (secs < 0) secs = 0;
    struct timespec req = { .tv_sec = (time_t)secs, .tv_nsec = 0 };
    nanosleep(&req, NULL);
    return true;
}