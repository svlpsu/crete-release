/***
 * Some code taken from Klee's file IO implementation.
 * Author: Christopher Havlicek
 **/

#define _LARGEFILE64_SOURCE // Required for Klee's file IO implementation on 32bit platforms.

#include <crete/host_harness.h>

#include <crete/test_case.h>

#include <fstream>
#include <string.h>
#include <assert.h>
#include <iostream> // testing.
#include <stdexcept>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "hook.h"
#include "klee/fd.h"

using namespace std;
using namespace crete;

extern "C" void crete_make_concolic_file_std(const char* name, size_t size);
extern "C" void crete_make_concolic_file_posix(const char* name, size_t size);
static void __create_new_dfile(exe_disk_file_t *dfile,
                               unsigned size,
                               const char* path);

const char* CRETE_TEST_CASE_COVERAGE_PATH = "test_case.bin";

static int g_argc = 0;
static char** g_argv = NULL;

static struct {
    sighandler_t abrt;
    sighandler_t fpe;
    sighandler_t hup;
    sighandler_t sint;
    sighandler_t ill;
    sighandler_t pipe;
    sighandler_t quit;
    sighandler_t segv;
    sighandler_t sys;
    sighandler_t term;
    sighandler_t tstp;
    sighandler_t xcpu;
    sighandler_t xfsz;
} g_previous_signal_handler;

const char* crete_signal_to_string(int signum)
{
    switch(signum)
    {
    case SIGABRT:
        return "SIGABRT";
    case SIGFPE:
        return "SIGFPE";
    case SIGHUP:
        return "SIGHUP";
    case SIGINT:
        return "SIGINT";
    case SIGILL:
        return "SIGILL";
    case SIGPIPE:
        return "SIGPIPE";
    case SIGQUIT:
        return "SIGQUIT";
    case SIGSEGV:
        return "SIGSEGV";
    case SIGSYS:
        return "SIGSYS";
    case SIGTERM:
        return "SIGTERM";
    case SIGTSTP:
        return "SIGTSTP";
    case SIGXCPU:
        return "SIGXCPU";
    case SIGXFSZ:
        return "SIGXFSZ";
    default:
        assert(0 && "signal string not found");
    }
}

void crete_call_previous_signal_handler(int signum)
{
    switch(signum)
    {
    case SIGABRT:
        signal(signum, g_previous_signal_handler.abrt);
        raise(signum);
        return;
    case SIGFPE:
        signal(signum, g_previous_signal_handler.fpe);
        raise(signum);
        return;
    case SIGHUP:
        signal(signum, g_previous_signal_handler.hup);
        raise(signum);
        return;
    case SIGINT:
        signal(signum, g_previous_signal_handler.sint);
        raise(signum);
        return;
    case SIGILL:
        signal(signum, g_previous_signal_handler.ill);
        raise(signum);
        return;
    case SIGPIPE:
        signal(signum, g_previous_signal_handler.pipe);
        raise(signum);
        return;
    case SIGQUIT:
        signal(signum, g_previous_signal_handler.quit);
        raise(signum);
        return;
    case SIGSEGV:
        signal(signum, g_previous_signal_handler.segv);
        raise(signum);
        return;
    case SIGSYS:
        signal(signum, g_previous_signal_handler.sys);
        raise(signum);
        return;
    case SIGTERM:
        signal(signum, g_previous_signal_handler.term);
        raise(signum);
        return;
    case SIGTSTP:
        signal(signum, g_previous_signal_handler.tstp);
        raise(signum);
        return;
    case SIGXCPU:
        signal(signum, g_previous_signal_handler.xcpu);
        raise(signum);
        return;
    case SIGXFSZ:
        signal(signum, g_previous_signal_handler.xfsz);
        raise(signum);
        return;
    default:
        assert(0 && "signal number not found");
    }
}

void crete_signal_callback_handler(int signum)
{
    namespace fs = boost::filesystem;

    const char* sigstr = crete_signal_to_string(signum);

    fs::path exe = fs::absolute(g_argv[0]);

    if(!fs::exists(exe))
    {
        throw runtime_error("[CRETE] failed to find executable path from argv[0]: " + exe.generic_string());
    }

    fs::path defect_dir = exe.parent_path() / "defect";

    if(!fs::exists(defect_dir))
    {
        if(!fs::create_directories(defect_dir))
        {
            throw runtime_error("[CRETE] failed to create directory: " + defect_dir.generic_string());
        }
    }

    fs::path report_path = defect_dir / (string("signal.") + sigstr);

    fs::ofstream report(report_path);

    report << "Signal caught: "
           << sigstr;

    crete_call_previous_signal_handler(signum);
}

void crete_initialize_signal_handlers()
{
    g_previous_signal_handler.abrt = signal(SIGABRT, crete_signal_callback_handler);
    g_previous_signal_handler.fpe = signal(SIGFPE, crete_signal_callback_handler);
    g_previous_signal_handler.hup = signal(SIGHUP, crete_signal_callback_handler);
    g_previous_signal_handler.sint = signal(SIGINT, crete_signal_callback_handler);
    g_previous_signal_handler.ill = signal(SIGILL, crete_signal_callback_handler);
    g_previous_signal_handler.pipe = signal(SIGPIPE, crete_signal_callback_handler);
    g_previous_signal_handler.quit = signal(SIGQUIT, crete_signal_callback_handler);
    g_previous_signal_handler.segv = signal(SIGSEGV, crete_signal_callback_handler);
    g_previous_signal_handler.sys = signal(SIGSYS, crete_signal_callback_handler);
    g_previous_signal_handler.term = signal(SIGTERM, crete_signal_callback_handler);
    g_previous_signal_handler.tstp = signal(SIGTSTP, crete_signal_callback_handler);
    g_previous_signal_handler.xcpu = signal(SIGXCPU, crete_signal_callback_handler);
    g_previous_signal_handler.xfsz = signal(SIGXFSZ, crete_signal_callback_handler);
}

extern "C" void crete_initialize(int argc, char* argv[])
{
    g_argc = argc;
    g_argv = argv;

    crete_initialize_signal_handlers();
}

extern "C" int crete_start(int (*harness)(int argc, char* argv[]))
{
    return harness(g_argc, g_argv);
}

extern "C" void crete_make_concolic(void* addr, size_t size, const char* name)
{
    static bool tc_initialized = false;
    static TestCase tc; // Needs to be static because it may be called multiple times in the same execution, but each execution will only need one test case.

    if(!tc_initialized)
    {
        ifstream ifs(CRETE_TEST_CASE_COVERAGE_PATH);
        if(!ifs)
            throw runtime_error((string("unable to open file: ") + CRETE_TEST_CASE_COVERAGE_PATH).c_str());

        tc = read_test_case(ifs);
        tc_initialized = true;
    }

    for(std::vector<TestCaseElement>::iterator iter = tc.get_elements().begin();
        iter != tc.get_elements().end();
        ++iter)
    {
        if(name == string(iter->name.begin(), iter->name.end()))
        {
            assert(size == iter->data.size());

            memcpy(addr,
                  (void*)&iter->data[0],
                   size);
        }
    }
}

extern "C" void crete_make_concolic_file(const char* name, size_t size)
{
    assert(strcmp(name, "") && "[CRETE] file name must be valid");
    assert(strlen(name) <= CRETE_MAX_FILENAME_SIZE && "[CRETE] file name size too large");
    assert(size && "[CRETE] file size must be greater than zero");

    crete_make_concolic_file_std(name, size);
    crete_make_concolic_file_posix(name, size);
}

extern "C" void crete_make_concolic_file_std(const char* name, size_t size)
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

extern "C" void crete_make_concolic_file_posix(const char* name, size_t size)
{
    static size_t file_index = 0;

    __create_new_dfile(&__exe_fs.sym_files[file_index],
                       size,
                       name);

    ++file_index;
}

extern "C" void crete_assume(int cond)
{
    assert(cond && "crete_assume condition failed!");
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

static void __create_new_dfile(exe_disk_file_t* dfile,
                               unsigned size,
                               const char* path)
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
  memcpy(&sname[sp-path], appendage, sizeof(appendage));

  assert(size);

  dfile->size = size;
  dfile->contents = (char*)malloc(dfile->size);
  crete_make_concolic(dfile->contents, dfile->size, sname);

  struct stat64* s = (struct stat64*)malloc(sizeof(struct stat64));
  if(stat64(path, s) == -1)
  {
      assert(0 && "[CRETE] Cannot stat file - does it exist? Permissions?");
  }
  dfile->stat = s;
}

int crete_posix_is_symbolic_fd(int fd)
{
    return __exe_env.fds[fd].dfile != NULL;
}

void crete_make_concolic_file_input(const char* name, size_t size, const uint8_t* input)
{
    (void)name;
    (void)size;
    (void)input;

    assert(0 && "TODO: crete_make_concolic_file_input");
}
