#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ops.h"
#include "exec.h"

bool op_out(VMThread *t, const Instruction *i) {
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    const char *s = regfile_get_string(t->regs, sreg);
    printf("%s", s ? s : "");
    fflush(stdout);
    return true;
}

bool op_inp(VMThread *t, const Instruction *i) {
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
        regfile_set_string(t->regs, sreg, "");
        return true;
    }
    /* Strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    regfile_set_string(t->regs, sreg, buf);
    return true;
}

bool op_dstr(VMThread *t, const Instruction *i) {
    /* DSTR <dest_S#> "<literal>" */
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    if (i->argc < 2 || i->args[1].type != ARG_IMM_STR) {
        vm_thread_flag(t, "line %d: DSTR requires a string literal", i->source_line);
        return false;
    }
    regfile_set_string(t->regs, sreg, i->args[1].u.imm_str);
    return true;
}

bool op_tcha(VMThread *t, const Instruction *i) {
    int64_t code; uint32_t creg;
    if (!exec_arg_to_int(t, &i->args[0], &code)) return false;
    if (!exec_arg_reg_c(t,  &i->args[1], &creg)) return false;
    t->regs->C[creg] = (char)(code & 0x7F);
    return true;
}

bool op_tint(VMThread *t, const Instruction *i) {
    uint32_t creg, dreg;
    if (!exec_arg_reg_c(t, &i->args[0], &creg)) return false;
    if (!exec_arg_reg_r(t, &i->args[1], &dreg)) return false;
    t->regs->R[dreg] = (int64_t)(unsigned char)t->regs->C[creg];
    return true;
}

bool op_ugst(VMThread *t, const Instruction *i) {
    /* UGST <S#> <C0> [C1 C2 ...] */
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    const char *str = regfile_get_string(t->regs, sreg);
    size_t slen = str ? strlen(str) : 0;

    for (int c = 1; c < i->argc; c++) {
        uint32_t creg;
        if (!exec_arg_reg_c(t, &i->args[c], &creg)) return false;
        size_t pos = (size_t)(c - 1);
        t->regs->C[creg] = (pos < slen) ? str[pos] : '\0';
    }
    return true;
}

bool op_gstr(VMThread *t, const Instruction *i) {
    /* GSTR <dest_S#> <C0> [C1 C2 ...] */
    uint32_t sreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;

    char buf[REG_C_COUNT + 1];
    int bi = 0;
    for (int c = 1; c < i->argc && bi < (int)sizeof(buf)-1; c++) {
        uint32_t creg;
        if (!exec_arg_reg_c(t, &i->args[c], &creg)) return false;
        buf[bi++] = t->regs->C[creg];
    }
    buf[bi] = '\0';
    regfile_set_string(t->regs, sreg, buf);
    return true;
}

bool op_sapp(VMThread *t, const Instruction *i) {
    /* SAPP <src_S#> <dest_S#> */
    uint32_t sreg_src, sreg_dst;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg_src)) return false;
    if (!exec_arg_reg_s(t, &i->args[1], &sreg_dst)) return false;

    const char *src = regfile_get_string(t->regs, sreg_src);
    const char *dst = regfile_get_string(t->regs, sreg_dst);
    size_t srclen = src ? strlen(src) : 0;
    size_t dstlen = dst ? strlen(dst) : 0;
    char *combined = malloc(srclen + dstlen + 1);
    if (!combined) return false;
    memcpy(combined, dst, dstlen);
    memcpy(combined + dstlen, src, srclen);
    combined[srclen + dstlen] = '\0';
    regfile_set_string(t->regs, sreg_dst, combined);
    free(combined);
    return true;
}

bool op_itos(VMThread *t, const Instruction *i) {
    int64_t val; uint32_t sreg;
    if (!exec_arg_to_int(t, &i->args[0], &val)) return false;
    if (!exec_arg_reg_s(t,  &i->args[1], &sreg)) return false;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)val);
    regfile_set_string(t->regs, sreg, buf);
    return true;
}

bool op_stoi(VMThread *t, const Instruction *i) {
    uint32_t sreg, dreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    if (!exec_arg_reg_r(t, &i->args[1], &dreg)) return false;
    const char *str = regfile_get_string(t->regs, sreg);
    if (!str || !*str) {
        vm_thread_flag(t, "line %d: STOI on empty string", i->source_line);
        return false;
    }
    char *end;
    long long val = strtoll(str, &end, 10);
    if (*end != '\0') {
        vm_thread_flag(t, "line %d: STOI cannot parse '%s'", i->source_line, str);
        return false;
    }
    t->regs->R[dreg] = (int64_t)val;
    return true;
}

bool op_wraw(VMThread *t, const Instruction *i) {
    /* Emit raw opcode and args to stdout */
    if (i->argc < 1) return true;
    int64_t opc;
    if (!exec_arg_to_int(t, &i->args[0], &opc)) return false;
    printf("0x%016llx", (unsigned long long)opc);
    for (int a = 1; a < i->argc; a++) {
        printf(" ");
        switch (i->args[a].type) {
            case ARG_REG_R:    printf("R%u",  i->args[a].u.reg_idx); break;
            case ARG_REG_S:    printf("S%u",  i->args[a].u.reg_idx); break;
            case ARG_REG_C:    printf("C%u",  i->args[a].u.reg_idx); break;
            case ARG_REG_L:    printf("L%u",  i->args[a].u.reg_idx); break;
            case ARG_REG_D:    printf("D%u",  i->args[a].u.reg_idx); break;
            case ARG_IMM_INT:  printf("%lld", (long long)i->args[a].u.imm_int); break;
            case ARG_IMM_STR:  printf("\"%s\"", i->args[a].u.imm_str); break;
            default: break;
        }
    }
    printf("\n");
    return true;
}

bool op_slen(VMThread *t, const Instruction *i) {
    uint32_t sreg, dreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    if (!exec_arg_reg_r(t, &i->args[1], &dreg)) return false;
    const char *str = regfile_get_string(t->regs, sreg);
    t->regs->R[dreg] = (int64_t)(str ? strlen(str) : 0);
    return true;
}

bool op_scmp(VMThread *t, const Instruction *i) {
    uint32_t sa, sb, dreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sa)) return false;
    if (!exec_arg_reg_s(t, &i->args[1], &sb)) return false;
    if (!exec_arg_reg_r(t, &i->args[2], &dreg)) return false;
    const char *a = regfile_get_string(t->regs, sa);
    const char *b = regfile_get_string(t->regs, sb);
    t->regs->R[dreg] = (strcmp(a ? a : "", b ? b : "") == 0) ? 1 : 0;
    return true;
}

bool op_isnum(VMThread *t, const Instruction *i) {
    uint32_t sreg, dreg;
    if (!exec_arg_reg_s(t, &i->args[0], &sreg)) return false;
    if (!exec_arg_reg_r(t, &i->args[1], &dreg)) return false;
    const char *str = regfile_get_string(t->regs, sreg);
    if (!str || !*str) { t->regs->R[dreg] = 0; return true; }
    const char *p = str;
    if (*p == '-' || *p == '+') p++;
    if (!*p) { t->regs->R[dreg] = 0; return true; }
    while (*p) {
        if (!isdigit((unsigned char)*p)) { t->regs->R[dreg] = 0; return true; }
        p++;
    }
    t->regs->R[dreg] = 1;
    return true;
}