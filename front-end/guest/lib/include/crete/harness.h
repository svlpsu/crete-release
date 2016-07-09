#ifndef CRETE_GUEST_H
#define CRETE_GUEST_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_initialize(int argc, char* argv[]);
int crete_start(int (*harness)(int argc, char* argv[]));

void crete_make_concolic(void* addr, size_t size, const char* name);
void crete_make_concolic_file(const char* name, size_t size);
void crete_make_concolic_file_input(const char* name, size_t size, const uint8_t* input);
void crete_initialize_concolic_posix_files(size_t file_count);
void crete_make_concolic_stdin(size_t size);

void crete_assume_begin();
void crete_assume_(int cond);

#define crete_assume(cond) \
    crete_assume_begin(); \
    crete_assume_(cond)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_GUEST_H
