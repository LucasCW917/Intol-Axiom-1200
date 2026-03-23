#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "vm.h"
#include "exec.h"

/* ── VM lifecycle ─────────────────────────────────────────── */

VM *vm_create(void) {
    VM *vm = calloc(1, sizeof(VM));
    if (!vm) return NULL;

    vm->mem_mgr = mem_manager_create();
    vm->ns_reg  = ns_registry_create();
    if (!vm->mem_mgr || !vm->ns_reg) { vm_destroy(vm); return NULL; }

    pthread_mutex_init(&vm->func_lock,   NULL);
    pthread_mutex_init(&vm->thread_lock, NULL);
    vm->next_thread_id = 1;
    return vm;
}

void vm_destroy(VM *vm) {
    if (!vm) return;

    /* Destroy all threads */
    VMThread *t = vm->threads;
    while (t) { VMThread *n = t->next; vm_thread_destroy(t); t = n; }

    /* Free function table */
    FuncEntry *fe = vm->func_table;
    while (fe) {
        FuncEntry *n = fe->next;
        free(fe->name);
        free(fe->file_path);
        if (fe->program) program_free(fe->program);
        free(fe);
        fe = n;
    }

    mem_manager_destroy(vm->mem_mgr);
    ns_registry_destroy(vm->ns_reg);
    pthread_mutex_destroy(&vm->func_lock);
    pthread_mutex_destroy(&vm->thread_lock);
    free(vm);
}

/* ── Thread lifecycle ─────────────────────────────────────── */

VMThread *vm_thread_create(VM *vm, Program *prog) {
    VMThread *t = calloc(1, sizeof(VMThread));
    if (!t) return NULL;

    t->vm      = vm;
    t->program = prog;
    t->regs    = regfile_create();
    if (!t->regs) { free(t); return NULL; }

    t->call_capacity = 64;
    t->call_stack    = malloc(t->call_capacity * sizeof(CallFrame));
    if (!t->call_stack) { regfile_destroy(t->regs); free(t); return NULL; }

    t->status  = VM_OK;
    t->running = true;
    pthread_mutex_init(&t->status_lock, NULL);

    pthread_mutex_lock(&vm->thread_lock);
    t->thread_id     = vm->next_thread_id++;
    t->next          = vm->threads;
    vm->threads      = t;
    pthread_mutex_unlock(&vm->thread_lock);

    return t;
}

void vm_thread_destroy(VMThread *t) {
    if (!t) return;
    regfile_destroy(t->regs);
    free(t->call_stack);
    free(t->error_msg);
    free(t->thrown_msg);
    pthread_mutex_destroy(&t->status_lock);
    free(t);
}

VMThread *vm_thread_find(VM *vm, uint32_t thread_id) {
    pthread_mutex_lock(&vm->thread_lock);
    for (VMThread *t = vm->threads; t; t = t->next) {
        if (t->thread_id == thread_id) {
            pthread_mutex_unlock(&vm->thread_lock);
            return t;
        }
    }
    pthread_mutex_unlock(&vm->thread_lock);
    return NULL;
}

/* ── Function table ───────────────────────────────────────── */

bool vm_register_func(VM *vm, uint64_t opcode, const char *name,
                      const char *file, int argc) {
    pthread_mutex_lock(&vm->func_lock);
    /* Check for duplicate */
    for (FuncEntry *fe = vm->func_table; fe; fe = fe->next) {
        if (fe->opcode == opcode) {
            pthread_mutex_unlock(&vm->func_lock);
            return false;
        }
    }
    FuncEntry *fe = calloc(1, sizeof(FuncEntry));
    if (!fe) { pthread_mutex_unlock(&vm->func_lock); return false; }
    fe->opcode    = opcode;
    fe->name      = strdup(name);
    fe->file_path = strdup(file);
    fe->argc      = argc;
    fe->next      = vm->func_table;
    vm->func_table = fe;
    pthread_mutex_unlock(&vm->func_lock);
    return true;
}

FuncEntry *vm_find_func(VM *vm, uint64_t opcode) {
    pthread_mutex_lock(&vm->func_lock);
    for (FuncEntry *fe = vm->func_table; fe; fe = fe->next) {
        if (fe->opcode == opcode) {
            pthread_mutex_unlock(&vm->func_lock);
            return fe;
        }
    }
    pthread_mutex_unlock(&vm->func_lock);
    return NULL;
}

/* ── Error helpers ────────────────────────────────────────── */

void vm_thread_flag(VMThread *t, const char *fmt, ...) {
    pthread_mutex_lock(&t->status_lock);
    t->status = VM_FLAGGED;
    free(t->error_msg);
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    t->error_msg = strdup(buf);
    fprintf(stderr, "[FLAGGED] %s\n", buf);
    pthread_mutex_unlock(&t->status_lock);
}

void vm_thread_crash(VMThread *t, const char *fmt, ...) {
    pthread_mutex_lock(&t->status_lock);
    t->status  = VM_CRASH;
    t->running = false;
    free(t->error_msg);
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    t->error_msg = strdup(buf);
    fprintf(stderr, "[CRASH] %s\n", buf);
    pthread_mutex_unlock(&t->status_lock);
}

void vm_thread_throw(VMThread *t, const char *msg) {
    free(t->thrown_msg);
    t->thrown_msg = strdup(msg ? msg : "");
    if (t->in_try) {
        longjmp(t->try_env, 1);
    } else {
        vm_thread_crash(t, "Unhandled exception: %s", msg ? msg : "");
    }
}

/* ── Top-level run ────────────────────────────────────────── */

VMStatus vm_run(VM *vm, Program *prog) {
    VMThread *t = vm_thread_create(vm, prog);
    if (!t) return VM_CRASH;
    VMStatus s = exec_thread(t);
    vm->status = s;

    /* Remove main thread from registry */
    pthread_mutex_lock(&vm->thread_lock);
    VMThread **pp = &vm->threads;
    while (*pp && *pp != t) pp = &(*pp)->next;
    if (*pp) *pp = t->next;
    pthread_mutex_unlock(&vm->thread_lock);

    vm_thread_destroy(t);
    return s;
}