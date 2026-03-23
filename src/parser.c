#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "parser.h"
#include "opcodes.h"

/* ── Token ────────────────────────────────────────────────── */
typedef struct {
    char   *text;
    int     line;
} Token;

typedef struct {
    Token  *toks;
    size_t  count;
    size_t  cap;
} TokenList;

static void tl_push(TokenList *tl, const char *text, int line) {
    if (tl->count >= tl->cap) {
        tl->cap  = tl->cap ? tl->cap * 2 : 64;
        tl->toks = realloc(tl->toks, tl->cap * sizeof(Token));
    }
    tl->toks[tl->count++] = (Token){ .text = strdup(text), .line = line };
}

static void tl_free(TokenList *tl) {
    for (size_t i = 0; i < tl->count; i++) free(tl->toks[i].text);
    free(tl->toks);
}

/* ── Lexer ────────────────────────────────────────────────── */
static TokenList lex(const char *src) {
    TokenList tl = {0};
    int line = 1;
    const char *p = src;

    while (*p) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;

        if (*p == '\n') { line++; p++; continue; }
        if (!*p) break;

        /* Comments: ; to end-of-line */
        if (*p == ';') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Quoted string literal */
        if (*p == '"') {
            p++;
            char buf[4096]; size_t bi = 0;
            while (*p && *p != '"' && bi < sizeof(buf) - 2) {
                if (*p == '\\') {
                    p++;
                    switch (*p) {
                        case 'n':  buf[bi++] = '\n'; break;
                        case 't':  buf[bi++] = '\t'; break;
                        case '"':  buf[bi++] = '"';  break;
                        case '\\': buf[bi++] = '\\'; break;
                        default:   buf[bi++] = *p;   break;
                    }
                } else buf[bi++] = *p;
                p++;
            }
            if (*p == '"') p++;
            buf[bi] = '\0';
            /* wrap in quotes so parser can distinguish */
            char quoted[4100];
            snprintf(quoted, sizeof(quoted), "\"%s\"", buf);
            tl_push(&tl, quoted, line);
            continue;
        }

        /* Regular token */
        char buf[512]; size_t bi = 0;
        while (*p && !isspace((unsigned char)*p) && *p != ';' && bi < sizeof(buf)-1)
            buf[bi++] = *p++;
        buf[bi] = '\0';
        if (bi) tl_push(&tl, buf, line);
    }
    return tl;
}

/* ── Opcode name table ────────────────────────────────────── */
typedef struct { const char *name; uint64_t opcode; } OpcodeEntry;

static const OpcodeEntry OPCODE_TABLE[] = {
    {"ADD",   OP_ADD},  {"SUB",   OP_SUB},  {"MUL",   OP_MUL},
    {"DIV",   OP_DIV},  {"SHL",   OP_SHL},  {"SHR",   OP_SHR},
    {"NEG",   OP_NEG},  {"SAV",   OP_SAV},  {"IFEQ",  OP_IFEQ},
    {"IFNQ",  OP_IFNQ}, {"IFTR",  OP_IFTR}, {"IFFL",  OP_IFFL},
    {"WHLE",  OP_WHLE}, {"DEF",   OP_DEF},  {"TAKE",  OP_TAKE},
    {"RET",   OP_RET},  {"STP",   OP_STP},  {"JMP",   OP_JMP},
    {"OUT",   OP_OUT},  {"INP",   OP_INP},  {"DSTR",  OP_DSTR},
    {"TCHA",  OP_TCHA}, {"TINT",  OP_TINT}, {"UGST",  OP_UGST},
    {"GSTR",  OP_GSTR}, {"SAPP",  OP_SAPP}, {"ITOS",  OP_ITOS},
    {"STOI",  OP_STOI}, {"WRAW",  OP_WRAW}, {"SAVL",  OP_SAVL},
    {"SAVD",  OP_SAVD}, {"GET",   OP_GET},  {"LEN",   OP_LEN},
    {"NAND",  OP_NAND}, {"NOT",   OP_NOT},  {"AND",   OP_AND},
    {"OR",    OP_OR},   {"NOR",   OP_NOR},  {"XOR",   OP_XOR},
    {"XNOR",  OP_XNOR}, {"BUF",   OP_BUF}, {"TRY",   OP_TRY},
    {"THW",   OP_THW},  {"GTM",   OP_GTM},  {"CTM",   OP_CTM},
    {"STAL",  OP_STAL}, {"FORK",  OP_FORK}, {"JOIN",  OP_JOIN},
    {"LOCK",  OP_LOCK}, {"UNLO",  OP_UNLO}, {"AADD",  OP_AADD},
    {"ASUB",  OP_ASUB}, {"AMUL",  OP_AMUL}, {"ADIV",  OP_ADIV},
    {"YIEL",  OP_YIEL}, {"THID",  OP_THID}, {"THST",  OP_THST},
    {"MOD",   OP_MOD},  {"ABS",   OP_ABS},  {"MIN",   OP_MIN},
    {"MAX",   OP_MAX},  {"SLEN",  OP_SLEN}, {"SCMP",  OP_SCMP},
    {"ISNUM", OP_ISNUM},{"CLRL",  OP_CLRL}, {"DELD",  OP_DELD},
    {"DBG",   OP_DBG},  {"BUFF",  OP_BUFF},
    {NULL, 0}
};

static uint64_t lookup_opcode(const char *name, bool *found) {
    for (int i = 0; OPCODE_TABLE[i].name; i++) {
        if (strcasecmp(OPCODE_TABLE[i].name, name) == 0) {
            *found = true;
            return OPCODE_TABLE[i].opcode;
        }
    }
    *found = false;
    return 0;
}

/* ── Arg parser ───────────────────────────────────────────── */
/* Parse one token into an Arg. Returns false on parse error. */
static bool parse_arg(const char *tok, Arg *out) {
    memset(out, 0, sizeof(*out));

    /* Quoted string literal */
    if (tok[0] == '"') {
        size_t len = strlen(tok);
        char *inner = strndup(tok + 1, len >= 2 ? len - 2 : 0);
        out->type = ARG_IMM_STR;
        out->u.imm_str = inner;
        return true;
    }

    /* Register types: R0, C0, S0, L0, D0, T0 */
    if ((tok[0]=='R'||tok[0]=='r') && isdigit((unsigned char)tok[1])) {
        out->type = ARG_REG_R;
        out->u.reg_idx = (uint32_t)atoi(tok + 1);
        return true;
    }
    if ((tok[0]=='C'||tok[0]=='c') && isdigit((unsigned char)tok[1])) {
        out->type = ARG_REG_C;
        out->u.reg_idx = (uint32_t)atoi(tok + 1);
        return true;
    }
    if ((tok[0]=='S'||tok[0]=='s') && isdigit((unsigned char)tok[1])) {
        out->type = ARG_REG_S;
        out->u.reg_idx = (uint32_t)atoi(tok + 1);
        return true;
    }
    if ((tok[0]=='T'||tok[0]=='t') && isdigit((unsigned char)tok[1])) {
        out->type = ARG_REG_T;
        out->u.reg_idx = (uint32_t)atoi(tok + 1);
        return true;
    }

    /* List with index: L0[3] */
    if ((tok[0]=='L'||tok[0]=='l') && isdigit((unsigned char)tok[1])) {
        const char *bracket = strchr(tok, '[');
        if (bracket) {
            out->type = ARG_REG_L_IDX;
            out->u.list_idx.reg      = (uint32_t)atoi(tok + 1);
            out->u.list_idx.elem_idx = (uint32_t)atoi(bracket + 1);
        } else {
            out->type = ARG_REG_L;
            out->u.reg_idx = (uint32_t)atoi(tok + 1);
        }
        return true;
    }

    /* Dictionary: D0 */
    if ((tok[0]=='D'||tok[0]=='d') && isdigit((unsigned char)tok[1])) {
        out->type = ARG_REG_D;
        out->u.reg_idx = (uint32_t)atoi(tok + 1);
        return true;
    }

    /* Hex immediate: 0x... */
    if (tok[0]=='0' && (tok[1]=='x'||tok[1]=='X')) {
        out->type = ARG_IMM_INT;
        out->u.imm_int = (int64_t)strtoull(tok, NULL, 16);
        return true;
    }

    /* Decimal immediate (possibly negative) */
    if (isdigit((unsigned char)tok[0]) ||
        (tok[0]=='-' && isdigit((unsigned char)tok[1]))) {
        out->type = ARG_IMM_INT;
        out->u.imm_int = (int64_t)strtoll(tok, NULL, 10);
        return true;
    }

    /* Opcode name used as argument (for DEF, FORK, etc.) */
    bool found = false;
    uint64_t opc = lookup_opcode(tok, &found);
    if (found) {
        out->type    = ARG_OPCODE;
        out->u.opcode = opc;
        return true;
    }

    /* Label / identifier */
    out->type      = ARG_LABEL;
    out->u.label   = strdup(tok);
    return true;
}

/* ── Instruction ──────────────────────────────────────────── */

void arg_free(Arg *a) {
    if (!a) return;
    if (a->type == ARG_IMM_STR)  { free(a->u.imm_str); a->u.imm_str = NULL; }
    if (a->type == ARG_LABEL)    { free(a->u.label);   a->u.label   = NULL; }
    a->type = ARG_NONE;
}

Arg arg_clone(const Arg *a) {
    Arg out = *a;
    if (a->type == ARG_IMM_STR && a->u.imm_str)
        out.u.imm_str = strdup(a->u.imm_str);
    if (a->type == ARG_LABEL && a->u.label)
        out.u.label = strdup(a->u.label);
    return out;
}

/* ── Program ──────────────────────────────────────────────── */

static void prog_push(Program *p, Instruction instr) {
    if (p->count >= p->capacity) {
        p->capacity = p->capacity ? p->capacity * 2 : 64;
        p->instrs   = realloc(p->instrs, p->capacity * sizeof(Instruction));
    }
    p->instrs[p->count++] = instr;
}

static void prog_add_label(Program *p, const char *name, size_t pos) {
    if (p->label_count >= p->label_cap) {
        p->label_cap    = p->label_cap ? p->label_cap * 2 : 16;
        p->label_names     = realloc(p->label_names, p->label_cap * sizeof(char*));
        p->label_positions = realloc(p->label_positions, p->label_cap * sizeof(size_t));
    }
    p->label_names[p->label_count]     = strdup(name);
    p->label_positions[p->label_count] = pos;
    p->label_count++;
}

bool program_find_label(const Program *p, const char *name, size_t *out_pos) {
    for (size_t i = 0; i < p->label_count; i++) {
        if (strcmp(p->label_names[i], name) == 0) {
            *out_pos = p->label_positions[i];
            return true;
        }
    }
    return false;
}

void program_free(Program *p) {
    if (!p) return;
    for (size_t i = 0; i < p->count; i++) {
        Instruction *instr = &p->instrs[i];
        for (int j = 0; j < instr->argc; j++) arg_free(&instr->args[j]);
        if (instr->guarded) {
            for (int j = 0; j < instr->guarded->argc; j++)
                arg_free(&instr->guarded->args[j]);
            free(instr->guarded);
        }
    }
    free(p->instrs);
    for (size_t i = 0; i < p->label_count; i++) free(p->label_names[i]);
    free(p->label_names);
    free(p->label_positions);
    free(p);
}

/* ── Token stream → Program ───────────────────────────────── */

/*  Opcodes that consume a guarded sub-instruction:
    IFTR <cond> <instruction...>
    IFFL <cond> <instruction...>
    WHLE <cond> <instruction...>                                  */
static bool is_guarding_opcode(uint64_t opc) {
    return opc == OP_IFTR || opc == OP_IFFL || opc == OP_WHLE;
}

/*  How many args does a given opcode take before the guarded sub-instr?
    IFTR/IFFL/WHLE each take 1 condition arg then the sub-instruction.   */
static int guarding_cond_count(uint64_t opc) {
    (void)opc;
    return 1;  /* all three take exactly 1 condition register */
}

static Program *tokens_to_program(TokenList *tl) {
    Program *p = calloc(1, sizeof(Program));
    if (!p) return NULL;

    size_t i = 0;
    while (i < tl->count) {
        const char *tok = tl->toks[i].text;
        int line = tl->toks[i].line;

        /* Look up opcode */
        bool found = false;
        uint64_t opc = lookup_opcode(tok, &found);
        if (!found) {
            fprintf(stderr, "parse error line %d: unknown opcode '%s'\n", line, tok);
            program_free(p);
            return NULL;
        }
        i++;

        Instruction instr = { .opcode = opc, .argc = 0, .source_line = line,
                              .guarded = NULL };

        /* STP: next token is label name */
        if (opc == OP_STP) {
            if (i >= tl->count) {
                fprintf(stderr, "parse error line %d: STP requires a label name\n", line);
                program_free(p); return NULL;
            }
            /* Register label pointing to the instruction AFTER STP */
            prog_add_label(p, tl->toks[i].text, p->count);
            i++;
            /* STP itself is a no-op at runtime; still emit it */
            Arg la = { .type = ARG_LABEL, .u.label = strdup(p->label_names[p->label_count-1]) };
            instr.args[0] = la; instr.argc = 1;
            prog_push(p, instr);
            continue;
        }

        /* For guarding opcodes collect condition arg(s) then sub-instruction */
        if (is_guarding_opcode(opc)) {
            int cond_count = guarding_cond_count(opc);
            for (int c = 0; c < cond_count && i < tl->count; c++, i++) {
                if (!parse_arg(tl->toks[i].text, &instr.args[instr.argc])) {
                    fprintf(stderr, "parse error line %d: bad arg '%s'\n",
                            line, tl->toks[i].text);
                    program_free(p); return NULL;
                }
                instr.argc++;
            }

            /* Sub-instruction: next token must be an opcode */
            if (i >= tl->count) {
                fprintf(stderr, "parse error line %d: missing guarded instruction\n", line);
                program_free(p); return NULL;
            }

            bool sf = false;
            uint64_t sub_opc = lookup_opcode(tl->toks[i].text, &sf);
            if (!sf) {
                fprintf(stderr, "parse error line %d: expected opcode after guard, got '%s'\n",
                        line, tl->toks[i].text);
                program_free(p); return NULL;
            }
            i++;

            Instruction *sub = calloc(1, sizeof(Instruction));
            sub->opcode = sub_opc;
            sub->source_line = line;

            /* Collect sub-instruction args until end of line or new opcode */
            while (i < tl->count) {
                bool sf2 = false;
                lookup_opcode(tl->toks[i].text, &sf2);
                /* Stop if we hit another opcode name that isn't a register */
                if (sf2 && tl->toks[i].text[0] != 'R' &&
                           tl->toks[i].text[0] != 'r' &&
                           tl->toks[i].text[0] != 'S' &&
                           tl->toks[i].text[0] != 's') {
                    break;
                }
                if (sub->argc >= MAX_ARGS) break;
                Arg a;
                if (!parse_arg(tl->toks[i].text, &a)) break;
                sub->args[sub->argc++] = a;
                i++;
            }
            instr.guarded = sub;
            prog_push(p, instr);
            continue;
        }

        /* General case: collect args until we see a new opcode or end */
        while (i < tl->count && instr.argc < MAX_ARGS) {
            const char *t = tl->toks[i].text;
            /* A token that is a known opcode name (but not a register-prefixed word)
               signals the start of the next instruction */
            bool sf = false;
            lookup_opcode(t, &sf);
            if (sf && t[0] != 'R' && t[0] != 'r' &&
                      t[0] != 'S' && t[0] != 's' &&
                      t[0] != 'C' && t[0] != 'c' &&
                      t[0] != 'L' && t[0] != 'l' &&
                      t[0] != 'D' && t[0] != 'd' &&
                      t[0] != 'T' && t[0] != 't') {
                break;
            }
            Arg a;
            if (!parse_arg(t, &a)) {
                fprintf(stderr, "parse error line %d: bad arg '%s'\n", line, t);
                program_free(p); return NULL;
            }
            instr.args[instr.argc++] = a;
            i++;
        }

        prog_push(p, instr);
    }

    return p;
}

/* ── Public API ───────────────────────────────────────────── */

Program *program_parse_string(const char *src) {
    TokenList tl = lex(src);
    Program  *p  = tokens_to_program(&tl);
    tl_free(&tl);
    return p;
}

Program *program_parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    Program *p = program_parse_string(buf);
    free(buf);
    return p;
}