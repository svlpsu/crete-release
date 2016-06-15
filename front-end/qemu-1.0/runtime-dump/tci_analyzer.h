#ifndef CRETE_TCI_ANALYZER_H
#define CRETE_TCI_ANALYZER_H

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif // __STDC_LIMIT_MACROS

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_tci_next_block(void);
bool crete_is_current_block_symbolic(void);
void crete_tci_make_symbolic(uint64_t addr, uint64_t size);
void crete_tci_mark_block_symbolic(void);
void crete_tci_next_iteration(void);

void crete_tci_reg_monitor_begin(void); // Must place at the entry of each instruction.
void crete_tci_read_reg(uint64_t index);
void crete_tci_write_reg(uint64_t index);

void crete_tci_ld8u_i32(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld_i32(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_st8_i32(uint64_t t1, uint64_t offset);
void crete_tci_st16_i32(uint64_t t1, uint64_t offset);
void crete_tci_st_i32(uint64_t t1, uint64_t offset);
void crete_tci_ld8u_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld32u_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld32s_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_st8_i64(uint64_t t1, uint64_t offset);
void crete_tci_st16_i64(uint64_t t1, uint64_t offset);
void crete_tci_st32_i64(uint64_t t1, uint64_t offset);
void crete_tci_st_i64(uint64_t t1, uint64_t offset);
void crete_tci_qemu_ld8u(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld8s(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld16u(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld16s(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld32u(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld32s(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld32(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld64(uint64_t t0, uint64_t addr);
void crete_tci_qemu_ld64_32(uint64_t t0, uint64_t t1, uint64_t addr);
void crete_tci_qemu_st8(uint64_t t0, uint64_t addr);
void crete_tci_qemu_st16(uint64_t t0, uint64_t addr);
void crete_tci_qemu_st32(uint64_t t0, uint64_t addr);
void crete_tci_qemu_st64(uint64_t addr);

// Trace Graph
bool crete_tci_is_block_branching(void);
void crete_tci_brcond(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_TCI_ANALYZER_H
