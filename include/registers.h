#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

/* ── Capacity constants (from spec) ───────────────────────── */
#define REG_R_COUNT   4096      /* 2^12 numeric registers      */
#define REG_C_COUNT   1024      /* 2^10 character registers    */
#define REG_S_COUNT   256       /* 2^8  string registers       */
#define REG_L_COUNT   16384     /* 2^14 list registers         */
#define REG_D_COUNT   4096      /* 2^12 dictionary registers   */
#define LIST_MAX_ITEMS 4096     /* 2^12 items per list         */
#define DICT_MAX_ENTRIES 1024   /* 2^10 entries per dict       */

/* ── List value type ──────────────────────────────────────── */
typedef enum {
    VAL_NONE = 0,
    VAL_INT,
    VAL_STR,
    VAL_LIST_REF,
    VAL_DICT_REF
} ValType;

typedef struct {
    ValType type;
    union {
        int64_t  i;
        char    *s;
        uint32_t list_idx;
        uint32_t dict_idx;
    } v;
} Value;

/* ── List register ────────────────────────────────────────── */
typedef struct {
    bool    allocated;
    Value  *items;
    size_t  count;
    size_t  capacity;
} ListReg;

/* ── Dictionary entry ─────────────────────────────────────── */
typedef struct DictEntry {
    char           *key;       /* string representation of key */
    Value           val;
    struct DictEntry *next;    /* chained for hash collisions  */
} DictEntry;

typedef struct {
    bool       allocated;
    DictEntry **buckets;
    size_t     bucket_count;
    size_t     entry_count;
} DictReg;

/* ── Register file ────────────────────────────────────────── */
typedef struct RegisterFile {
    int64_t  R[REG_R_COUNT];    /* numeric                     */
    char     C[REG_C_COUNT];    /* character (ASCII)           */
    char    *S[REG_S_COUNT];    /* string (heap-allocated)     */
    ListReg  L[REG_L_COUNT];    /* list                        */
    DictReg  D[REG_D_COUNT];    /* dictionary                  */

    /* per-register mutexes for LOCK/UNLO */
    pthread_mutex_t R_lock[REG_R_COUNT];
    pthread_mutex_t L_lock[REG_L_COUNT];
    pthread_mutex_t D_lock[REG_D_COUNT];
} RegisterFile;

/* ── Register file lifecycle ──────────────────────────────── */
RegisterFile *regfile_create(void);
void          regfile_destroy(RegisterFile *rf);

/* ── String register helpers ──────────────────────────────── */
bool regfile_set_string(RegisterFile *rf, uint32_t idx, const char *str);
const char *regfile_get_string(const RegisterFile *rf, uint32_t idx);
void regfile_free_string(RegisterFile *rf, uint32_t idx);

/* ── List register helpers ────────────────────────────────── */
bool regfile_list_append(RegisterFile *rf, uint32_t idx, Value val);
bool regfile_list_get(const RegisterFile *rf, uint32_t idx, size_t pos, Value *out);
size_t regfile_list_len(const RegisterFile *rf, uint32_t idx);
void regfile_list_clear(RegisterFile *rf, uint32_t idx);

/* ── Dictionary register helpers ──────────────────────────── */
bool regfile_dict_set(RegisterFile *rf, uint32_t idx, const char *key, Value val);
bool regfile_dict_get(const RegisterFile *rf, uint32_t idx, const char *key, Value *out);
bool regfile_dict_del(RegisterFile *rf, uint32_t idx, const char *key);
size_t regfile_dict_len(const RegisterFile *rf, uint32_t idx);

/* ── Value helpers ────────────────────────────────────────── */
Value value_int(int64_t i);
Value value_str(const char *s);   /* duplicates s */
void  value_free(Value *v);
Value value_clone(const Value *v);

#endif /* REGISTERS_H */