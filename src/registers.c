#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "registers.h"

/* ── Value helpers ────────────────────────────────────────── */

Value value_int(int64_t i) {
    return (Value){ .type = VAL_INT, .v.i = i };
}

Value value_str(const char *s) {
    Value v = { .type = VAL_STR, .v.s = s ? strdup(s) : NULL };
    return v;
}

void value_free(Value *v) {
    if (!v) return;
    if (v->type == VAL_STR) { free(v->v.s); v->v.s = NULL; }
    v->type = VAL_NONE;
}

Value value_clone(const Value *v) {
    if (!v) return (Value){0};
    if (v->type == VAL_STR) return value_str(v->v.s);
    return *v;
}

/* ── Register file lifecycle ──────────────────────────────── */

RegisterFile *regfile_create(void) {
    RegisterFile *rf = calloc(1, sizeof(RegisterFile));
    if (!rf) return NULL;

    for (uint32_t i = 0; i < REG_R_COUNT; i++)
        pthread_mutex_init(&rf->R_lock[i], NULL);
    for (uint32_t i = 0; i < REG_L_COUNT; i++)
        pthread_mutex_init(&rf->L_lock[i], NULL);
    for (uint32_t i = 0; i < REG_D_COUNT; i++)
        pthread_mutex_init(&rf->D_lock[i], NULL);

    return rf;
}

void regfile_destroy(RegisterFile *rf) {
    if (!rf) return;

    /* Free all strings */
    for (int i = 0; i < REG_S_COUNT; i++) {
        free(rf->S[i]);
        rf->S[i] = NULL;
    }

    /* Free all lists */
    for (int i = 0; i < REG_L_COUNT; i++) {
        if (rf->L[i].allocated) regfile_list_clear(rf, i);
        free(rf->L[i].items);
    }

    /* Free all dicts */
    for (int i = 0; i < REG_D_COUNT; i++) {
        if (!rf->D[i].allocated) continue;
        for (size_t b = 0; b < rf->D[i].bucket_count; b++) {
            DictEntry *e = rf->D[i].buckets[b];
            while (e) {
                DictEntry *next = e->next;
                free(e->key);
                value_free(&e->val);
                free(e);
                e = next;
            }
        }
        free(rf->D[i].buckets);
    }

    /* Destroy mutexes */
    for (uint32_t i = 0; i < REG_R_COUNT; i++)
        pthread_mutex_destroy(&rf->R_lock[i]);
    for (uint32_t i = 0; i < REG_L_COUNT; i++)
        pthread_mutex_destroy(&rf->L_lock[i]);
    for (uint32_t i = 0; i < REG_D_COUNT; i++)
        pthread_mutex_destroy(&rf->D_lock[i]);

    free(rf);
}

/* ── String register helpers ──────────────────────────────── */

bool regfile_set_string(RegisterFile *rf, uint32_t idx, const char *str) {
    if (idx >= REG_S_COUNT) return false;
    free(rf->S[idx]);
    rf->S[idx] = str ? strdup(str) : NULL;
    return true;
}

const char *regfile_get_string(const RegisterFile *rf, uint32_t idx) {
    if (idx >= REG_S_COUNT) return NULL;
    return rf->S[idx] ? rf->S[idx] : "";
}

void regfile_free_string(RegisterFile *rf, uint32_t idx) {
    if (idx >= REG_S_COUNT) return;
    free(rf->S[idx]);
    rf->S[idx] = NULL;
}

/* ── List register helpers ────────────────────────────────── */

bool regfile_list_append(RegisterFile *rf, uint32_t idx, Value val) {
    if (idx >= REG_L_COUNT) return false;
    ListReg *lr = &rf->L[idx];

    if (!lr->allocated) {
        lr->items    = malloc(8 * sizeof(Value));
        if (!lr->items) return false;
        lr->capacity = 8;
        lr->count    = 0;
        lr->allocated = true;
    }

    if (lr->count >= LIST_MAX_ITEMS) return false;

    if (lr->count >= lr->capacity) {
        size_t newcap = lr->capacity * 2;
        if (newcap > LIST_MAX_ITEMS) newcap = LIST_MAX_ITEMS;
        Value *newbuf = realloc(lr->items, newcap * sizeof(Value));
        if (!newbuf) return false;
        lr->items    = newbuf;
        lr->capacity = newcap;
    }

    lr->items[lr->count++] = value_clone(&val);
    return true;
}

bool regfile_list_get(const RegisterFile *rf, uint32_t idx, size_t pos, Value *out) {
    if (idx >= REG_L_COUNT) return false;
    const ListReg *lr = &rf->L[idx];
    if (!lr->allocated || pos >= lr->count) return false;
    *out = value_clone(&lr->items[pos]);
    return true;
}

size_t regfile_list_len(const RegisterFile *rf, uint32_t idx) {
    if (idx >= REG_L_COUNT || !rf->L[idx].allocated) return 0;
    return rf->L[idx].count;
}

void regfile_list_clear(RegisterFile *rf, uint32_t idx) {
    if (idx >= REG_L_COUNT || !rf->L[idx].allocated) return;
    ListReg *lr = &rf->L[idx];
    for (size_t i = 0; i < lr->count; i++) value_free(&lr->items[i]);
    lr->count = 0;
}

/* ── Dictionary register helpers ──────────────────────────── */

#define DICT_BUCKETS 64

static uint32_t dict_hash(const char *key) {
    uint32_t h = 2166136261u;
    while (*key) { h ^= (unsigned char)*key++; h *= 16777619u; }
    return h;
}

static bool dict_ensure_init(RegisterFile *rf, uint32_t idx) {
    DictReg *dr = &rf->D[idx];
    if (dr->allocated) return true;
    dr->buckets = calloc(DICT_BUCKETS, sizeof(DictEntry *));
    if (!dr->buckets) return false;
    dr->bucket_count = DICT_BUCKETS;
    dr->entry_count  = 0;
    dr->allocated    = true;
    return true;
}

bool regfile_dict_set(RegisterFile *rf, uint32_t idx, const char *key, Value val) {
    if (idx >= REG_D_COUNT) return false;
    if (!dict_ensure_init(rf, idx)) return false;

    DictReg *dr = &rf->D[idx];
    uint32_t h  = dict_hash(key) % dr->bucket_count;

    /* Update existing */
    for (DictEntry *e = dr->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            value_free(&e->val);
            e->val = value_clone(&val);
            return true;
        }
    }

    /* Insert new */
    if (dr->entry_count >= DICT_MAX_ENTRIES) return false;
    DictEntry *ne = malloc(sizeof(DictEntry));
    if (!ne) return false;
    ne->key  = strdup(key);
    ne->val  = value_clone(&val);
    ne->next = dr->buckets[h];
    dr->buckets[h] = ne;
    dr->entry_count++;
    return true;
}

bool regfile_dict_get(const RegisterFile *rf, uint32_t idx,
                      const char *key, Value *out) {
    if (idx >= REG_D_COUNT || !rf->D[idx].allocated) return false;
    const DictReg *dr = &rf->D[idx];
    uint32_t h = dict_hash(key) % dr->bucket_count;
    for (DictEntry *e = dr->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { *out = value_clone(&e->val); return true; }
    }
    return false;
}

bool regfile_dict_del(RegisterFile *rf, uint32_t idx, const char *key) {
    if (idx >= REG_D_COUNT || !rf->D[idx].allocated) return false;
    DictReg *dr = &rf->D[idx];
    uint32_t h  = dict_hash(key) % dr->bucket_count;
    DictEntry **pp = &dr->buckets[h];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            DictEntry *del = *pp;
            *pp = del->next;
            free(del->key);
            value_free(&del->val);
            free(del);
            dr->entry_count--;
            return true;
        }
        pp = &(*pp)->next;
    }
    return false;
}

size_t regfile_dict_len(const RegisterFile *rf, uint32_t idx) {
    if (idx >= REG_D_COUNT || !rf->D[idx].allocated) return 0;
    return rf->D[idx].entry_count;
}