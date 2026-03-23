#include <stdlib.h>
#include <stdio.h>
#include "mem_manager.h"

MemManager *mem_manager_create(void) {
    MemManager *mm = calloc(1, sizeof(MemManager));
    if (!mm) return NULL;
    mm->b0 = 1;   /* B0 itself occupies slot 0; idle value is 1 */
    pthread_mutex_init(&mm->lock, NULL);
    return mm;
}

void mem_manager_destroy(MemManager *mm) {
    if (!mm) return;
    BufferBlock *b = mm->head;
    while (b) { BufferBlock *n = b->next; free(b); b = n; }
    pthread_mutex_destroy(&mm->lock);
    free(mm);
}

uint32_t mem_manager_alloc(MemManager *mm, uint32_t count, void *owner) {
    if (!count) return UINT32_MAX;
    pthread_mutex_lock(&mm->lock);

    uint32_t base = mm->b0;
    BufferBlock *blk = malloc(sizeof(BufferBlock));
    if (!blk) { pthread_mutex_unlock(&mm->lock); return UINT32_MAX; }

    blk->base  = base;
    blk->count = count;
    blk->owner = owner;
    blk->next  = mm->head;
    mm->head   = blk;
    mm->b0    += count;

    pthread_mutex_unlock(&mm->lock);
    return base;
}

bool mem_manager_free(MemManager *mm, uint32_t base, void *owner) {
    pthread_mutex_lock(&mm->lock);

    /* Find and remove the block */
    BufferBlock **pp = &mm->head;
    BufferBlock  *found = NULL;
    while (*pp) {
        if ((*pp)->base == base && (*pp)->owner == owner) {
            found = *pp;
            *pp   = found->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (!found) { pthread_mutex_unlock(&mm->lock); return false; }

    uint32_t freed_count = found->count;
    uint32_t freed_base  = found->base;
    free(found);

    /*  Compaction: shift all blocks whose base > freed_base downward.
        This keeps the buffer space contiguous (no gaps below B0).       */
    for (BufferBlock *b = mm->head; b; b = b->next) {
        if (b->base > freed_base) {
            b->base -= freed_count;
        }
    }

    mm->b0 -= freed_count;

    pthread_mutex_unlock(&mm->lock);
    return true;
}

uint32_t mem_manager_b0(MemManager *mm) {
    pthread_mutex_lock(&mm->lock);
    uint32_t v = mm->b0;
    pthread_mutex_unlock(&mm->lock);
    return v;
}