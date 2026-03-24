#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Argument type ────────────────────────────────────────── */
typedef enum {
    ARG_NONE = 0,
    ARG_REG_R,
    ARG_REG_C,
    ARG_REG_S,
    ARG_REG_L,
    ARG_REG_D,
    ARG_REG_T,
    ARG_REG_L_IDX,
    ARG_IMM_INT,
    ARG_IMM_STR,
    ARG_LABEL,
    ARG_OPCODE,
} ArgType;

typedef struct {
    ArgType type;
    union {
        uint32_t reg_idx;
        int64_t  imm_int;
        char    *imm_str;
        char    *label;
        uint64_t opcode;
        struct { uint32_t reg; uint32_t elem_idx; } list_idx;
    } u;
} Arg;

/* ── Instruction ──────────────────────────────────────────── */
#define MAX_ARGS 16

typedef struct Instruction {
    uint64_t            opcode;
    Arg                 args[MAX_ARGS];
    int                 argc;
    int                 source_line;
    struct Instruction *guarded;
} Instruction;

/* ── Program ──────────────────────────────────────────────── */
typedef struct {
    Instruction *instrs;
    size_t       count;
    size_t       capacity;
    char       **label_names;
    size_t      *label_positions;
    size_t       label_count;
    size_t       label_cap;
} Program;

/* ── Parser API ───────────────────────────────────────────── */
Program *program_parse_file(const char *path);
Program *program_parse_string(const char *src);
void     program_free(Program *p);
bool     program_find_label(const Program *p, const char *name, size_t *out_pos);
void     arg_free(Arg *a);
Arg      arg_clone(const Arg *a);

#endif /* PARSER_H */