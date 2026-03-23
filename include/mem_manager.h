#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

/*  Buffer Register System
    ──────────────────────
    B# registers are opaque to user code.  The MEM MANAGER core owns
    the entire buffer address space and enforces:
      • Contiguous allocation (no gaps below B0)
      • Atomic compaction when out-of-order frees occur
      • B0 (BCR) always reflects the current high-water mark          */

typedef struct BufferBlock {
    uint32_t base;    /* first buffer index in this block  */
    uint32_t count;   /* number of buffers in this block   */
    void    *owner;   /* opaque owner token (vm_thread ptr)*/
    struct BufferBlock *next;
} BufferBlock;

typedef struct {
    uint32_t     b0;           /* B0: Buffer Count Register            */
    BufferBlock *head;         /* linked list of allocated blocks       */
    pthread_mutex_t lock;      /* protects all fields (atomic compact.) */
} MemManager;

/* ── Lifecycle ────────────────────────────────────────────── */
MemManager *mem_manager_create(void);
void        mem_manager_destroy(MemManager *mm);

/* ── Allocation / deallocation ───────────────────────────── */
/* Returns base address of the allocated block, or UINT32_MAX on failure */
uint32_t mem_manager_alloc(MemManager *mm, uint32_t count, void *owner);

/* Free a block previously allocated by owner. Triggers compaction. */
bool mem_manager_free(MemManager *mm, uint32_t base, void *owner);

/* Query current B0 value */
uint32_t mem_manager_b0(MemManager *mm);

#endif /* MEM_MANAGER_H */