#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/*  Namespace registry
    ──────────────────
    Namespace 0 is reserved for the CPU (built-in opcodes).
    Programs register namespaces 1–4294967295.
    Assignments are unique and collision-checked at registration time.  */

#define NS_CPU 0u

typedef struct NsEntry {
    uint32_t       ns_id;
    char          *program_name;
    struct NsEntry *next;
} NsEntry;

typedef struct {
    NsEntry        *head;
    uint32_t        next_id;    /* monotonically increasing allocator */
    pthread_mutex_t lock;
} NamespaceRegistry;

/* ── Lifecycle ────────────────────────────────────────────── */
NamespaceRegistry *ns_registry_create(void);
void               ns_registry_destroy(NamespaceRegistry *reg);

/* Register a new program namespace; returns assigned ns_id or 0 on failure */
uint32_t ns_register(NamespaceRegistry *reg, const char *program_name);

/* Look up a namespace id by program name; returns 0 if not found */
uint32_t ns_lookup(NamespaceRegistry *reg, const char *program_name);

/* Check if an opcode (full 64-bit) belongs to namespace 0 (built-in) */
bool ns_is_builtin(uint64_t opcode);

#endif /* NAMESPACE_H */