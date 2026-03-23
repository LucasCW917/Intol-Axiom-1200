#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Argument type ────────────────────────────────────────── */
typedef enum {
    ARG_NONE = 0,
    ARG_REG_R,      /* Rn  */
    ARG_REG_C,      /* Cn  */
    ARG_REG_S,      /* Sn  */
    ARG_REG_L,      /* Ln  */
    ARG_REG_D,      /* Dn  */
    ARG_REG_T,      /* Tn  */
    ARG_REG_L_IDX,  /* Ln[i] — list index access */
    ARG_IMM_INT,    /* literal integer            */
    ARG_IMM_STR,    /* literal string (for DSTR)  */
    ARG_LABEL,      /* jump target name           */
    ARG_OPCODE,     /* raw opcode integer         */
} ArgType;

typedef struct {
    ArgType type;
    union {
        uint32_t reg_idx;           /* register number */
        int64_t  imm_int;           /* immediate int   */
        char    *imm_str;           /* immediate str   */
        char    *label;             /* STP/JMP name    */
        uint64_t opcode;            /* raw opcode      */
        struct { uint32_t reg; uint32_t elem_idx; } list_idx;
    } u;
} Arg;

/* ── Instruction ──────────────────────────────────────────── */
#define MAX_ARGS 16

typedef struct {
    uint64_t opcode;
    Arg      args[MAX_ARGS];
    int      argc;
    int      source_line;  /* for error messages */
    /* For IFTR/IFFL/WHLE: the guarded sub-instruction */
    struct Instruction *guarded;
} Instruction;

/* ── Program (compiled instruction array) ─────────────────── */
typedef struct {
    Instruction *instrs;
    size_t       count;
    size_t       capacity;

    /* label map: name → instruction index */
    char   **label_names;
    size_t  *label_positions;
    size_t   label_count;
    size_t   label_cap;
} Program;

/* ── Parser API ───────────────────────────────────────────── */
Program *program_parse_file(const char *path);
Program *program_parse_string(const char *src);
void     program_free(Program *p);

/* Label resolution */
bool     program_find_label(const Program *p, const char *name, size_t *out_pos);

/* Arg helpers */
void arg_free(Arg *a);
Arg  arg_clone(const Arg *a);

#endif /* PARSER_H */