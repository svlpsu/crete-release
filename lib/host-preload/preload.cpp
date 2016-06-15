#include "preload.h"

#include <crete/test_case.h>
#include <crete/harness_config.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp> // Needed for text_iarchive (for some reason).
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <dlfcn.h>
#include <cassert>
#include <cstdlib>

#include <iostream>
#include <string>
#include <stdexcept>
#include <stdio.h>

#if defined(GUEST_REPLAY)
#include <crete/custom_instr.h>
#endif // defined(GUEST_REPLAY)

using namespace std;
using namespace crete;
namespace fs = boost::filesystem;

const char* const crete_test_case_args_path = "test_case_args.bin";
const char* const crete_proc_maps_path = "proc-maps.log";

/// Note: relies on the order of the test case - not name.
void crete_process_test_case(int& argc, char**& argv)
{
    if(!fs::exists(crete_test_case_args_path))
        throw std::runtime_error(std::string("failed to find file: ") + crete_test_case_args_path);

    fs::ifstream ifs(crete_test_case_args_path);

    if(!ifs.good())
        throw std::runtime_error(std::string("failed to open file: ") + crete_test_case_args_path);

    config::Arguments args;
    boost::archive::text_iarchive ia(ifs);
    ia >> args;

    char** orig_argv = argv;

    argc = args.size() + 1; //  +1 for argv[0].
    argv = (char**)malloc((sizeof(char*) * argc) + (sizeof(char*) * 2)); // +1 for argv[argc] = NULL.

    argv[0] = (char*)malloc(sizeof(char) * strlen(orig_argv[0]) + 1);
    strcpy(argv[0], orig_argv[0]);
    argv[argc] = NULL;

    for(config::Arguments::const_iterator it = args.begin();
        it != args.end();
        ++it)
    {
        const config::Argument& arg = *it;

        assert(arg.size != 0 && "[libcrete_host_preload.so] arg size should never be 0!");

        char* ca = (char*)malloc(sizeof(char) * arg.value.size() + 1);
        memset((void*)ca, 0, arg.value.size() + 1);

        std::copy(arg.value.begin(),
                  arg.value.end(),
                  ca);

        config::Arguments::const_iterator b = args.begin(); // No vector::cbegin in c++03...
        std::size_t idx = std::distance(b, it);
        ++idx; // argv[0] is program name, so offset by 1.

        argv[idx] = ca;
    }
}

#if defined(GUEST_REPLAY)
void crete_write_proc_maps(void)
{
    if(!fs::exists(crete_proc_maps_path))
    {
        fs::ifstream ifs("/proc/self/maps");
        fs::ofstream ofs(crete_proc_maps_path);

        copy(istreambuf_iterator<char>(ifs),
             istreambuf_iterator<char>(),
             ostreambuf_iterator<char>(ofs));
    }
}

void crete_set_up_custom_instructions(void)
{
    atexit(crete_capture_end);

    crete_capture_begin();
}
#endif // defined(GUEST_REPLAY)

void crete_preload_initialize(int& argc, char**& argv)
{
#if defined(GUEST_REPLAY)
    if(!fs::exists(crete_proc_maps_path)) // priming only
    {
        crete_write_proc_maps();
        exit(0);
    }
    crete_set_up_custom_instructions();
#endif // defined(GUEST_REPLAY)

    crete_process_test_case(argc, argv);
}

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end) {

    __libc_start_main_t orig_libc_start_main;

    try
    {
        orig_libc_start_main = (__libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");

        if(orig_libc_start_main == 0)
            throw runtime_error("failed to find __libc_start_main");

#if defined(GUEST_REPLAY)
            crete_preload_initialize(argc, ubp_av);
#else // !defined(GUEST_REPLAY)
        printf("prog: $$$ %s\n", ubp_av[0]);
        if(strcmp(ubp_av[0], "valgrind") != 0 &&
           strcmp(ubp_av[0], "fgrep") != 0 &&
           strcmp(ubp_av[0], "/bin/sh") != 0 &&
           strcmp(ubp_av[0], "/usr/bin/valgrind.bin") != 0 )
        {
            crete_preload_initialize(argc, ubp_av);
        }
#endif // defined(GUEST_REPLAY)

    }
    catch(exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
        exit(1);
    }
    catch(...) // Non-standard exception
    {
        cerr << "[CRETE] Non-standard exception encountered!" << endl;
        assert(0);
    }

    for(int i = 0; i < argc; ++i)
    {
        cout << "argv[" << i << "]: " << ubp_av[i] << endl;
    }


    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached. Doesn't matter, as atexit() is called upon returning from main().
}
