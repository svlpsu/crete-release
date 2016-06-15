#ifndef CUSTOM_INSTRUCTIONS_H
#define CUSTOM_INSTRUCTIONS_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************/
/* Functions for QEMU c code */
void bct_tcg_emit_custom_instruction(uint64_t arg);
void bct_tcg_emit_crete_make_symbolic(uint64_t arg);
#if defined(HAVLICEK_ASSUME) || 1
void crete_emit_assume(uint64_t arg);
#endif // HAVLICEK_ASSUME || 1

void crete_set_data_dir(const char* data_dir);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

enum CreteFileType {
    CRETE_FILE_TYPE_LLVM_LIB,
    CRETE_FILE_TYPE_LLVM_TEMPLATE,
};

extern std::string crete_data_dir;

std::string crete_find_file(CreteFileType type, const char* name);
#endif

#endif
