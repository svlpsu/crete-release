#ifndef QEMU_GLOBALS_H
#define QEMU_GLOBALS_H

#include <stdint.h>

extern "C" {

// Exception index copied from qemu/i386-softmmu/cpu.h
#define EXCP00_DIVZ	0
#define EXCP01_DB	1
#define EXCP02_NMI	2
#define EXCP03_INT3	3
#define EXCP04_INTO	4
#define EXCP05_BOUND	5
#define EXCP06_ILLOP	6
#define EXCP07_PREX	7
#define EXCP08_DBLE	8
#define EXCP09_XERR	9
#define EXCP0A_TSS	10
#define EXCP0B_NOSEG	11
#define EXCP0C_STACK	12
#define EXCP0D_GPF	13
#define EXCP0E_PAGE	14
#define EXCP10_COPR	16
#define EXCP11_ALGN	17
#define EXCP12_MCHK	18

#if !defined(CRETE_QEMU10)
extern int qemu_loglevel;
extern struct _IO_FILE *qemu_logfile;
#else
extern int loglevel;
extern struct _IO_FILE *logfile;

extern const uint8_t parity_table[256];

/* modulo 9 table */
extern const uint8_t rclb_table[32];

/* modulo 17 table */
extern const uint8_t rclw_table[32];

typedef void CPUWriteMemoryFunc(void *opaque, uint64_t addr, uint32_t value);
typedef uint32_t CPUReadMemoryFunc(void *opaque, uint64_t addr);
extern CPUWriteMemoryFunc *io_mem_write[512][4];
extern CPUReadMemoryFunc *io_mem_read[512][4];

extern void *io_mem_opaque[512];

extern int use_icount;
#endif // #if !defined(QEMU10) && 0

} // extern "C"

#endif
