#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include "ops.h"
#include "exec.h"

/* ── Thread entry point ───────────────────────────────────── */

typedef struct {
    VMThread   *thread;
    uint64_t    func_opcode;
} ForkArgs;

static void *fork_entry(void *arg) {
    ForkArgs *fa = arg;
    VMThread *t  = fa->thread;
    free(fa);

    exec_thread(t);

    pthread_mutex_lock(&t->status_lock);
    t->running = false;
    pthread_mutex_unlock(&t->status_lock);

    return NULL;
}

/* ── FORK ─────────────────────────────────────────────────── */
/*  FORK <T#> <R#|immediate>
    Spawns a thread that runs the function whose opcode is stored
    in the given R# or immediate.  Stores the thread ID in T#.   */
bool op_fork(VMThread *t, const Instruction *i) {
    uint32_t treg;
    if (!exec_arg_reg_t(t, &i->args[0], &treg)) return false;

    int64_t opc_raw;
    if (!exec_arg_to_int(t, &i->args[1], &opc_raw)) return false;
    uint64_t func_opcode = (uint64_t)opc_raw;

    FuncEntry *fe = vm_find_func(t->vm, func_opcode);
    if (!fe) {
        vm_thread_flag(t, "line %d: FORK: unknown function opcode 0x%016llx",
                       i->source_line, (unsigned long long)func_opcode);
        return false;
    }

    /* Lazily compile the function program */
    if (!fe->program) {
        fe->program = program_parse_file(fe->file_path);
        if (!fe->program) {
            vm_thread_flag(t, "line %d: FORK: failed to compile '%s'",
                           i->source_line, fe->file_path);
            return false;
        }
    }

    VMThread *child = vm_thread_create(t->vm, fe->program);
    if (!child) {
        vm_thread_flag(t, "line %d: FORK: failed to create thread", i->source_line);
        return false;
    }

    ForkArgs *fa = malloc(sizeof(ForkArgs));
    if (!fa) { vm_thread_destroy(child); return false; }
    fa->thread       = child;
    fa->func_opcode  = func_opcode;

    if (pthread_create(&child->pthread, NULL, fork_entry, fa) != 0) {
        free(fa);
        vm_thread_destroy(child);
        vm_thread_flag(t, "line %d: FORK: pthread_create failed", i->source_line);
        return false;
    }

    /* Store thread_id into T# — we use R# to hold thread IDs since T# has no
       value store of its own; the spec says T# registers are "returned by FORK". */
    t->regs->R[treg] = (int64_t)child->thread_id;
    return true;
}

/* ── JOIN ─────────────────────────────────────────────────── */
bool op_join(VMThread *t, const Instruction *i) {
    uint32_t treg;
    if (!exec_arg_reg_t(t, &i->args[0], &treg)) return false;
    uint32_t tid = (uint32_t)t->regs->R[treg];

    VMThread *child = vm_thread_find(t->vm, tid);
    if (!child) {
        /* Thread already finished and was cleaned up — that's fine */
        return true;
    }
    pthread_join(child->pthread, NULL);

    /* Remove from VM thread list and destroy */
    pthread_mutex_lock(&t->vm->thread_lock);
    VMThread **pp = &t->vm->threads;
    while (*pp && *pp != child) pp = &(*pp)->next;
    if (*pp) *pp = child->next;
    pthread_mutex_unlock(&t->vm->thread_lock);

    vm_thread_destroy(child);
    return true;
}

/* ── LOCK / UNLO ──────────────────────────────────────────── */
bool op_lock(VMThread *t, const Instruction *i) {
    const Arg *a = &i->args[0];
    switch (a->type) {
        case ARG_REG_R:
            if (a->u.reg_idx >= REG_R_COUNT) return false;
            pthread_mutex_lock(&t->regs->R_lock[a->u.reg_idx]);
            return true;
        case ARG_REG_L:
            if (a->u.reg_idx >= REG_L_COUNT) return false;
            pthread_mutex_lock(&t->regs->L_lock[a->u.reg_idx]);
            return true;
        case ARG_REG_D:
            if (a->u.reg_idx >= REG_D_COUNT) return false;
            pthread_mutex_lock(&t->regs->D_lock[a->u.reg_idx]);
            return true;
        default:
            vm_thread_flag(t, "line %d: LOCK only supports R#, L#, D#", i->source_line);
            return false;
    }
}

bool op_unlo(VMThread *t, const Instruction *i) {
    const Arg *a = &i->args[0];
    switch (a->type) {
        case ARG_REG_R:
            if (a->u.reg_idx >= REG_R_COUNT) return false;
            pthread_mutex_unlock(&t->regs->R_lock[a->u.reg_idx]);
            return true;
        case ARG_REG_L:
            if (a->u.reg_idx >= REG_L_COUNT) return false;
            pthread_mutex_unlock(&t->regs->L_lock[a->u.reg_idx]);
            return true;
        case ARG_REG_D:
            if (a->u.reg_idx >= REG_D_COUNT) return false;
            pthread_mutex_unlock(&t->regs->D_lock[a->u.reg_idx]);
            return true;
        default:
            vm_thread_flag(t, "line %d: UNLO only supports R#, L#, D#", i->source_line);
            return false;
    }
}

/* ── Atomic math helpers ──────────────────────────────────── */
/*  We use the per-register mutex to provide atomicity.
    This is correct but heavier than true hardware atomics;
    it's sufficient for emulation purposes.                    */

static bool atomic_binop(VMThread *t, const Instruction *i,
                         int64_t (*op)(int64_t, int64_t, VMThread*, int)) {
    int64_t a, b; uint32_t dest;
    if (!exec_arg_to_int(t, &i->args[0], &a)) return false;
    if (!exec_arg_to_int(t, &i->args[1], &b)) return false;
    if (!exec_arg_reg_r(t,  &i->args[2], &dest)) return false;

    pthread_mutex_lock(&t->regs->R_lock[dest]);
    int64_t result = op(a, b, t, i->source_line);
    if (t->status == VM_OK) t->regs->R[dest] = result;
    pthread_mutex_unlock(&t->regs->R_lock[dest]);
    return t->status == VM_OK;
}

static int64_t _add(int64_t a, int64_t b, VMThread *t, int ln) { (void)t;(void)ln; return a+b; }
static int64_t _sub(int64_t a, int64_t b, VMThread *t, int ln) { (void)t;(void)ln; return a-b; }
static int64_t _mul(int64_t a, int64_t b, VMThread *t, int ln) { (void)t;(void)ln; return a*b; }
static int64_t _div(int64_t a, int64_t b, VMThread *t, int ln) {
    if (b == 0) { vm_thread_flag(t, "line %d: ADIV by zero", ln); return 0; }
    return a / b;
}

bool op_aadd(VMThread *t, const Instruction *i) { return atomic_binop(t, i, _add); }
bool op_asub(VMThread *t, const Instruction *i) { return atomic_binop(t, i, _sub); }
bool op_amul(VMThread *t, const Instruction *i) { return atomic_binop(t, i, _mul); }
bool op_adiv(VMThread *t, const Instruction *i) { return atomic_binop(t, i, _div); }

/* ── YIEL ─────────────────────────────────────────────────── */
bool op_yiel(VMThread *t, const Instruction *i) {
    (void)t; (void)i;
    sched_yield();
    return true;
}

/* ── THID ─────────────────────────────────────────────────── */
bool op_thid(VMThread *t, const Instruction *i) {
    uint32_t dreg;
    if (!exec_arg_reg_r(t, &i->args[0], &dreg)) return false;
    t->regs->R[dreg] = (int64_t)t->thread_id;
    return true;
}

/* ── THST ─────────────────────────────────────────────────── */
bool op_thst(VMThread *t, const Instruction *i) {
    uint32_t treg, dreg;
    if (!exec_arg_reg_t(t, &i->args[0], &treg)) return false;
    if (!exec_arg_reg_r(t, &i->args[1], &dreg)) return false;

    uint32_t tid = (uint32_t)t->regs->R[treg];
    VMThread *target = vm_thread_find(t->vm, tid);
    if (!target) {
        t->regs->R[dreg] = 0;   /* not found → finished */
        return true;
    }
    pthread_mutex_lock(&target->status_lock);
    int64_t running = target->running ? 1 : 0;
    pthread_mutex_unlock(&target->status_lock);
    t->regs->R[dreg] = running;
    return true;
}