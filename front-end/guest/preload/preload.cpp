#include "preload.h"
#include "argv_processor.h"

#include <crete/harness.h>
#include <crete/custom_instr.h>
#include <crete/harness_config.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp> // Needed for text_iarchive (for some reason).
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>

#include <dlfcn.h>
#include <cassert>
#include <cstdlib>

#include <iostream>
#include <string>
#include <stdexcept>
#include <stdio.h>

#if CRETE_HOST_ENV
#include "run_config.h"
#endif // CRETE_HOST_ENV

using namespace std;
using namespace crete;
namespace fs = boost::filesystem;

const char* const crete_config_file = "harness.config.serialized";
const std::string crete_proc_maps_file = "proc-maps.log";

config::HarnessConfiguration crete_load_configuration()
{
    std::ifstream ifs(crete_config_file,
                      ios_base::in | ios_base::binary);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + std::string(crete_config_file));
    }

    boost::archive::text_iarchive ia(ifs);
    config::HarnessConfiguration config;
    ia >> config;

    return config;
}

void crete_process_stdin(const config::HarnessConfiguration& hconfig)
{
    const config::STDStream stdin_config = hconfig.get_stdin();
    if(stdin_config.concolic && stdin_config.size > 0)
    {
        crete_make_concolic_stdin(stdin_config.size);
    }
}

void crete_process_files(const config::HarnessConfiguration& hconfig)
{
    const config::Files& files = hconfig.get_files();
    const size_t file_count = files.size();

    if(file_count == 0)
        return;

    // Klee's POSIX file implmentation needs to be allocated at one time.
    crete_initialize_concolic_posix_files(file_count);

    for(config::Files::const_iterator it = files.begin();
        it != files.end();
        ++it)
    {
        const config::File& file = *it;
        std::size_t size = file.size;

        crete_make_concolic_file(file.path.string().c_str(), size);
    }
}

void crete_process_configuration(const config::HarnessConfiguration& hconfig,
                                 int& argc, char**& argv)
{
    // Note: order matters.
    config::process_argv(hconfig.get_arguments(),
                         hconfig.is_first_iteration(),
                         argc, argv);
    crete_process_files(hconfig);
    crete_process_stdin(hconfig);
}

void crete_preload_initialize(int& argc, char**& argv)
{
    crete_initialize(argc, argv);
    // Need to call crete_capture_begin before make_concolics, or they won't be captured.
    // crete_capture_end() is registered with atexit() in crete_initialize().
    crete_capture_begin();

    config::HarnessConfiguration hconfig = crete_load_configuration();
    crete_process_configuration(hconfig, argc, argv);
}

#if CRETE_HOST_ENV
bool crete_verify_executable_path_matches(const char* argv0)
{
    std::ifstream ifs(crete_config_file,
                      ios_base::in | ios_base::binary);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + std::string(crete_config_file));
    }

    boost::archive::text_iarchive ia(ifs);
    config::RunConfiguration config;
    ia >> config;

    return fs::equivalent(config.get_executable(), fs::path(argv0));
}
#endif // CRETE_HOST_ENV

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end) {

    std::cerr << "in crete_preload" << std::endl;

    __libc_start_main_t orig_libc_start_main;

    try
    {
        orig_libc_start_main = (__libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");
        if(orig_libc_start_main == 0)
            throw runtime_error("failed to find __libc_start_main");

//        void* afunc = (void*)dlsym(RTLD_DEFAULT, "av_parse_time");
//        (void)afunc;

#if CRETE_HOST_ENV
        // HACK (a bit of one)...
        // TODO: (crete-memcheck) research how to get around the problem of preloading
        // valgrind as well, in which case we check if the executable name matches.
        if(crete_verify_executable_path_matches(ubp_av[0]))
        {
            crete_preload_initialize(argc, ubp_av);
        }
#else
        crete_preload_initialize(argc, ubp_av);
#endif // CRETE_HOST_ENV

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


    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached. Doesn't matter, as atexit() is called upon returning from main().
}
