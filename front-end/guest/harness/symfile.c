/***
 * Some code taken from Klee's file IO implementation.
 * Author: Christopher Havlicek
 **/

#include <assert.h>

#include <crete/harness.h>
#include <crete/custom_instr.h>
#include <hook.h>
#include <sys/stat.h>
#include "klee/fd.h"

static void crete_make_concolic_file_std(const char* name,
                                         size_t size);
static void crete_make_concolic_file_posix(const char* name,
                                           size_t size);
static void __create_new_dfile(exe_disk_file_t *dfile,
                               unsigned size,
                               const char* path,
                               struct stat64* defaults);

void crete_make_concolic_file(const char* name, size_t size)
{
    assert(strcmp(name, "") && "[CRETE] file name must be valid");
    assert(strlen(name) <= CRETE_MAX_FILENAME_SIZE && "[CRETE] file name size too large");
    assert(size && "[CRETE] file size must be greater than zero");

    // Since we don't know what file routines will be used (e.g., open() vs fopen()),
    // initialize both kinds.
    crete_make_concolic_file_std(name, size);
    crete_make_concolic_file_posix(name, size);
}

// TODO: I reckon this is redundant. I don't think this is necessary,
// as I send initial inputs to dispatch, which then propagates it to the VM.
void crete_make_concolic_file_input(const char* name,
                                    size_t size,
                                    const uint8_t* input)
{
    assert(strcmp(name, "") && "[CRETE] file name must be valid");
    assert(strlen(name) <= CRETE_MAX_FILENAME_SIZE && "[CRETE] file name size too large");
    assert(size && "[CRETE] file size must be greater than zero");

    CreteConcolicFile* file = crete_make_blank_file();

    file->name = malloc(strlen(name) + 1);

    assert(file->name);

    strcpy(file->name, name);

    file->data = malloc(size);

    assert(file->data);

    file->size = size;

    // TODO: make check to ensure we don't make duplicate concolic files.

    crete_make_concolic(file->data, file->size, file->name);

    // TODO: should probably ensure that this isn't captured (we need the data to remain concolic):
    // crete_capture_disable(); // Ensure, if this were ever somehow called during a capture phase, that it's not captured.
    memcpy(file->data, input, file->size);
    // crete_capture_enable();
}

static void crete_make_concolic_file_std(const char* name, size_t size)
{
    assert(strcmp(name, "") && "[CRETE] file name must be valid");
    assert(strlen(name) <= CRETE_MAX_FILENAME_SIZE && "[CRETE] file name size too large");
    assert(size && "[CRETE] file size must be greater than zero");

    CreteConcolicFile* file = crete_make_blank_file();

    file->name = (char*)malloc(strlen(name) + 1);

    assert(file->name);

    strcpy(file->name, name);

    file->data = (uint8_t*)malloc(size);

    assert(file->data);

    file->size = size;

    // TODO: make check to ensure we don't make duplicate concolic files.

    crete_make_concolic(file->data, file->size, file->name);
}

void crete_make_concolic_stdin_std(size_t size)
{
    assert(size && "[CRETE] file size must be greater than zero");

    CRETE_FILE* file = crete_make_blank_concolic_stdin();

    file->size = size;
    file->contents = (uint8_t*)malloc(file->size);

    assert(file->contents);

    file->stream_pos = &file->contents[0];
    file->stream_end = &file->contents[file->size - 1];

    // TODO: make check to ensure we don't make duplicate concolic files.

    crete_make_concolic(file->contents, file->size, "crete-stdin");
}

void crete_make_concolic_stdin_posix(size_t size)
{
    assert(size && "[CRETE] file size must be greater than zero");
    assert(__exe_fs.sym_stdin == NULL && "[CRETE] stdin already initialized");

    const char* const name = "crete-stdin"; // __create_new_dfile appends -posix.

    struct stat64* s = (struct stat64*)malloc(sizeof(struct stat64));
    stat64(".", s);

    __exe_fs.sym_stdin = malloc(sizeof(*__exe_fs.sym_stdin));
    __create_new_dfile(__exe_fs.sym_stdin, size, name, s);
    __exe_env.fds[0].dfile = __exe_fs.sym_stdin; // STDIN_FILENO == 0

}

void crete_make_concolic_stdin(size_t size)
{
    assert(size && "[CRETE] file size must be greater than zero");

    crete_make_concolic_stdin_std(size);
    crete_make_concolic_stdin_posix(size);
}

static void crete_make_concolic_file_posix(const char* name, size_t size)
{
    static size_t file_index = 0;

    struct stat64* s = (struct stat64*)malloc(sizeof(struct stat64));
    stat64(".", s);

    __create_new_dfile(&__exe_fs.sym_files[file_index],
                       size,
                       name,
                       s);

    ++file_index;
}

static void __create_new_dfile(exe_disk_file_t* dfile,
                               unsigned size,
                               const char* path,
                               struct stat64 *defaults)
{
  const char *sp;
  char sname[64];

  size_t path_len = strlen(path);

  assert(path_len < sizeof(sname));

  dfile->path = (char*)malloc(path_len + 1);
  strcpy(dfile->path, path);

  for (sp=path; *sp; ++sp)
    sname[sp-path] = *sp;

  const char* const appendage = "-posix";
  memcpy(&sname[sp-path], appendage, 7);

  assert(size);

  dfile->size = size;
  dfile->contents = (char*)malloc(dfile->size);
  crete_make_concolic(dfile->contents, dfile->size, sname);

  struct stat64* s = (struct stat64*)malloc(sizeof(struct stat64));
  if(stat64(path, s) != -1)
  {
    dfile->stat = s;
  }
  else
  {
    dfile->stat = defaults;
  }
}

void crete_initialize_concolic_posix_files(size_t file_count)
{
    __exe_fs.n_sym_files = file_count;
    __exe_fs.sym_files = (exe_disk_file_t*)calloc(file_count, sizeof(*__exe_fs.sym_files));

    __exe_fs.sym_stdin = NULL;
    __exe_fs.sym_stdout = NULL;
    __exe_fs.max_failures = 0;
    __exe_fs.read_fail = NULL;
    __exe_fs.write_fail = NULL;
    __exe_fs.close_fail = NULL;
    __exe_fs.ftruncate_fail = NULL;
    __exe_fs.getcwd_fail = NULL;
    __exe_fs.chmod_fail = NULL;
    __exe_fs.fchmod_fail = NULL;

    __exe_env.version = 1;
    __exe_env.save_all_writes = 0; // Truncate writes to the original file size.
    __exe_env.umask = 0755; // Is this used? Educated guess here.
}
