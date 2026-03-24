#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <setjmp.h>

#include "registers.h"
#include "mem_manager.h"
#include "namespace.h"
#include "parser.h"

/* ── VM status codes ──────────────────────────────────────── */
typedef enum {
    VM_OK = 0,
    VM_FLAGGED,        /* division by zero, bad parse, etc.   */
    VM_EXCEPTION,      /* THW was called outside TRY          */
    VM_CRASH,          /* illegal B# access or fatal error    */
    VM_HALTED,         /* program finished normally           */
} VMStatus;

/* ── Forward declarations ─────────────────────────────────── */
struct VM;
struct VMThread;

/* ── User-defined function entry ──────────────────────────── */
typedef struct FuncEntry {
    uint64_t        opcode;     /* opcode this function is bound to   */
    char           *name;
    char           *file_path;  /* .apo source file                   */
    Program        *program;    /* lazily compiled on first call       */
    int             argc;
    struct FuncEntry *next;
} FuncEntry;

/* ── Thread state (one per FORK + one for main) ───────────── */
typedef struct VMThread {
    uint32_t         thread_id;
    pthread_t        pthread;
    struct VM       *vm;          /* back-pointer to owning VM          */

    /* each thread has its own register file for isolation */
    RegisterFile    *regs;
    Program         *program;
    size_t           ip;          /* instruction pointer                */

    /* call stack for functions */
    struct CallFrame *call_stack;
    int               call_depth;
    int               call_capacity;

    VMStatus         status;
    char            *error_msg;   /* heap-allocated, NULL if none       */

    /* exception support (TRY/THW) */
    bool             in_try;
    jmp_buf          try_env;
    char            *thrown_msg;  /* message from THW                   */

    /* thread management */
    bool             running;
    pthread_mutex_t  status_lock;
    struct VMThread *next;        /* intrusive list in VM               */
} VMThread;

/* ── Call frame (for DEF/TAKE/RET) ───────────────────────── */
typedef struct CallFrame {
    Program *program;             /* the function's program          */
    size_t   ret_ip;              /* return instruction pointer      */
    Program *ret_program;         /* return program context          */
    int      argc;                /* number of args passed           */
    Arg      args[MAX_ARGS];      /* copies of passed arguments      */
} CallFrame;

/* ── VM global state ──────────────────────────────────────── */
typedef struct VM {
    MemManager        *mem_mgr;
    NamespaceRegistry *ns_reg;

    /* function table */
    FuncEntry         *func_table;
    pthread_mutex_t    func_lock;

    /* thread registry */
    VMThread          *threads;
    uint32_t           next_thread_id;
    pthread_mutex_t    thread_lock;

    /* global status */
    VMStatus           status;
} VM;

/* ── VM lifecycle ─────────────────────────────────────────── */
VM      *vm_create(void);
void     vm_destroy(VM *vm);

/* Run a program on the VM; returns final status */
VMStatus vm_run(VM *vm, Program *prog);

/* ── Thread operations ────────────────────────────────────── */
VMThread *vm_thread_create(VM *vm, Program *prog);
void      vm_thread_destroy(VMThread *t);
VMThread *vm_thread_find(VM *vm, uint32_t thread_id);

/* ── Function table ───────────────────────────────────────── */
bool      vm_register_func(VM *vm, uint64_t opcode, const char *name,
                           const char *file, int argc);
FuncEntry *vm_find_func(VM *vm, uint64_t opcode);

/* ── Error helpers ────────────────────────────────────────── */
void vm_thread_flag(VMThread *t, const char *fmt, ...);
void vm_thread_crash(VMThread *t, const char *fmt, ...);
void vm_thread_throw(VMThread *t, const char *msg);

#endif /* VM_H */