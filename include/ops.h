#ifndef OPS_H
#define OPS_H

#include "vm.h"
#include "parser.h"

/* Each family returns true on success, false on error.
   Errors are reported via vm_thread_flag / vm_thread_crash. */

/* ── ops_math.c ───────────────────────────────────────────── */
bool op_add (VMThread *t, const Instruction *i);
bool op_sub (VMThread *t, const Instruction *i);
bool op_mul (VMThread *t, const Instruction *i);
bool op_div (VMThread *t, const Instruction *i);
bool op_shl (VMThread *t, const Instruction *i);
bool op_shr (VMThread *t, const Instruction *i);
bool op_neg (VMThread *t, const Instruction *i);
bool op_sav (VMThread *t, const Instruction *i);
bool op_mod (VMThread *t, const Instruction *i);
bool op_abs (VMThread *t, const Instruction *i);
bool op_min (VMThread *t, const Instruction *i);
bool op_max (VMThread *t, const Instruction *i);

/* ── ops_logic.c ──────────────────────────────────────────── */
bool op_ifeq(VMThread *t, const Instruction *i);
bool op_ifnq(VMThread *t, const Instruction *i);
bool op_iftr(VMThread *t, const Instruction *i);
bool op_iffl(VMThread *t, const Instruction *i);
bool op_whle(VMThread *t, const Instruction *i);

/* ── ops_io.c ─────────────────────────────────────────────── */
bool op_out (VMThread *t, const Instruction *i);
bool op_inp (VMThread *t, const Instruction *i);
bool op_dstr(VMThread *t, const Instruction *i);
bool op_tcha(VMThread *t, const Instruction *i);
bool op_tint(VMThread *t, const Instruction *i);
bool op_ugst(VMThread *t, const Instruction *i);
bool op_gstr(VMThread *t, const Instruction *i);
bool op_sapp(VMThread *t, const Instruction *i);
bool op_itos(VMThread *t, const Instruction *i);
bool op_stoi(VMThread *t, const Instruction *i);
bool op_wraw(VMThread *t, const Instruction *i);
bool op_slen(VMThread *t, const Instruction *i);
bool op_scmp(VMThread *t, const Instruction *i);
bool op_isnum(VMThread *t, const Instruction *i);

/* ── ops_collections.c ────────────────────────────────────── */
bool op_savl(VMThread *t, const Instruction *i);
bool op_savd(VMThread *t, const Instruction *i);
bool op_get (VMThread *t, const Instruction *i);
bool op_len (VMThread *t, const Instruction *i);
bool op_clrl(VMThread *t, const Instruction *i);
bool op_deld(VMThread *t, const Instruction *i);

/* ── ops_bitwise.c ────────────────────────────────────────── */
bool op_nand(VMThread *t, const Instruction *i);
bool op_not (VMThread *t, const Instruction *i);
bool op_and (VMThread *t, const Instruction *i);
bool op_or  (VMThread *t, const Instruction *i);
bool op_nor (VMThread *t, const Instruction *i);
bool op_xor (VMThread *t, const Instruction *i);
bool op_xnor(VMThread *t, const Instruction *i);
bool op_buf (VMThread *t, const Instruction *i);

/* ── ops_exceptions.c ─────────────────────────────────────── */
bool op_try (VMThread *t, const Instruction *i);
bool op_thw (VMThread *t, const Instruction *i);

/* ── ops_time.c ───────────────────────────────────────────── */
bool op_gtm (VMThread *t, const Instruction *i);
bool op_ctm (VMThread *t, const Instruction *i);
bool op_stal(VMThread *t, const Instruction *i);

/* ── ops_concurrency.c ────────────────────────────────────── */
bool op_fork(VMThread *t, const Instruction *i);
bool op_join(VMThread *t, const Instruction *i);
bool op_lock(VMThread *t, const Instruction *i);
bool op_unlo(VMThread *t, const Instruction *i);
bool op_aadd(VMThread *t, const Instruction *i);
bool op_asub(VMThread *t, const Instruction *i);
bool op_amul(VMThread *t, const Instruction *i);
bool op_adiv(VMThread *t, const Instruction *i);
bool op_yiel(VMThread *t, const Instruction *i);
bool op_thid(VMThread *t, const Instruction *i);
bool op_thst(VMThread *t, const Instruction *i);

/* ── ops_functions.c ──────────────────────────────────────── */
bool op_def (VMThread *t, const Instruction *i);
bool op_ret (VMThread *t, const Instruction *i);
bool op_call_user(VMThread *t, uint64_t opcode, const Instruction *i);

/* ── ops_branch.c ─────────────────────────────────────────── */
bool op_stp (VMThread *t, const Instruction *i);  /* no-op at runtime */
bool op_jmp (VMThread *t, const Instruction *i);

/* ── ops_debug.c ──────────────────────────────────────────── */
bool op_dbg (VMThread *t, const Instruction *i);

#endif /* OPS_H */