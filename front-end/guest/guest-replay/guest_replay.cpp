#include "guest_replay.h"

#include "crete/custom_instr.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/assign.hpp>

#include <boost/process.hpp>

#include <string>

using namespace std;
using namespace boost;
namespace fs = filesystem;
namespace po = program_options;
namespace pt = property_tree;
namespace bp = boost::process;

namespace crete
{

const std::string PROC_MAPS_FILE_NAME = "proc-maps.log";

GuestReplayExecutor::GuestReplayExecutor(const filesystem::path& binary,
                                         const filesystem::path& test_case_dir,
                                         const boost::filesystem::path& configuration,
                                         bool trace_mode) :
    Executor(binary,
             test_case_dir,
             configuration),
    trace_mode_(trace_mode),
    libc_main_found_(false),
    libc_exit_found_(false)
{
    if(trace_mode_)
    {
        initialize_configuration(configuration);

        initialize_proc_reader();

        prime();
        set_call_depth();
        load_defaults();
        process_filters();
        process_function_entries();

    }
}

GuestReplayExecutor::~GuestReplayExecutor()
{
    reset_timer();
}

// Note: this is the clean function for each iteration.
void GuestReplayExecutor::clean()
{
    Executor::clean();
}

void GuestReplayExecutor::send_dump_instr()
{
//    crete_send_custom_instr_dump(); // Causes call stack error.
    std::system("crete-dump");
}


void GuestReplayExecutor::execute()
{
    start_timer();
    Executor::execute();
    stop_timer();

    send_dump_instr();
}

void GuestReplayExecutor::initialize_configuration(const filesystem::path& configuration)
{
    pt::ptree rconfig;

    pt::read_xml(configuration.string(), rconfig);

    config_ = config::RunConfiguration(rconfig);
}

void GuestReplayExecutor::initialize_proc_reader()
{
    const char* const proc_maps_file = "proc-maps.log";

    fs::remove(proc_maps_file);

    bp::context ctx;
    ctx.work_directory = working_directory().generic_string();
    ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    ctx.stdout_behavior = bp::capture_stream();
    ctx.environment = bp::self::get_environment();
    ctx.environment.erase("LD_PRELOAD");
    ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_host_preload.so"));

    std::vector<std::string> args = boost::assign::list_of(binary().filename().generic_string());

    bp::child proc = bp::launch(binary(), args, ctx);

    bp::pistream& is = proc.get_stdout();

    std::string line;
    while(getline(is, line))
    {
        std::cout << line << std::endl;
    }

    proc.wait();

    proc_reader_ = ProcReader(proc_maps_file);
}

void GuestReplayExecutor::reset_timer()
{
    crete_send_custom_instr_reset_stopwatch();
}

void GuestReplayExecutor::start_timer()
{
    crete_send_custom_instr_start_stopwatch();
}

void GuestReplayExecutor::stop_timer()
{
    crete_send_custom_instr_stop_stopwatch();
}

void GuestReplayExecutor::prime()
{
    crete_send_custom_instr_prime();
}

void GuestReplayExecutor::set_call_depth()
{
    crete_insert_instr_call_stack_size(config_.get_exploration().call_depth);
}

void GuestReplayExecutor::load_defaults()
{
    const char* const default_function_includes[] = {
            "__crete_make_symbolic",
            "__crete_make_symbolic_internal",
            "__crete_touch_buffer",
            "crete_assume_",
            "crete_assume_begin",
            "crete_make_concrete",
            "crete_debug_print_f32",
            "crete_debug_print_buf",
            "crete_debug_assert_is_concolic",
            "crete_debug_monitor_concolic_status",
            "crete_debug_monitor_value",
            "crete_debug_monitor_set_flag" };
    const char* const default_function_excludes[] = {
            "crete_capture_begin",
            "crete_capture_end" };
    const char* const default_call_stack_sections[] = {
        ".plt",
        ".got.plt",
#if __x86_64__
        ".rela.plt"
#else // !__x86_64__
        ".rel.plt"
#endif // __x86_64__
    };

    std::vector<std::string> default_libraries;

    const char* const vm_comm_lib = "libcrete_vm_comm.so.1.0.0"; // TODO: should deduce from only .so, so the version isn't hard coded here. Possibily making use of fs::equivalent()
    const char* const preload_lib = "libcrete_preload.so";
    const char* const hook_lib = "/usr/lib/libcrete_hook.so.1.0.0"; // TODO: should deduce from only .so, so the version isn't hard coded here. Possibily making use of fs::equivalent()

    pt::ptree preloads_config;
    pt::ptree libs_config;
    pt::ptree func_include_config;
    pt::ptree func_exclude_config;
    pt::ptree sections_exclude_config;

    {
        pt::ptree& node = preloads_config.add_child("preloads.preload", pt::ptree());
        node.put("<xmlattr>.path", preload_lib);
    }

    config_.load_preloads(preloads_config);

    for(std::vector<std::string>::const_iterator it = default_libraries.begin();
        it != default_libraries.end();
        ++it)
    {
        pt::ptree& node = libs_config.add_child("libs.lib", pt::ptree());
        node.put("<xmlattr>.path", *it);
    }

    config_.load_libraries(libs_config);

    for(std::size_t i = 0;
        (i < sizeof(default_function_includes) / sizeof(char*));
        ++i)
    {
        const char* name = default_function_includes[i];

        pt::ptree& node = func_include_config.add_child("funcs.include.func", pt::ptree());
        node.put("<xmlattr>.name", name);
        node.put("<xmlattr>.lib", vm_comm_lib);
    }

    config_.load_functions(func_include_config);

    for(std::size_t i = 0;
        (i < sizeof(default_function_excludes) / sizeof(char*));
        ++i)
    {
        const char* name = default_function_excludes[i];

        pt::ptree& node = func_exclude_config.add_child("funcs.exclude.func", pt::ptree());
        node.put("<xmlattr>.name", name);
        node.put("<xmlattr>.lib", vm_comm_lib);
    }

    config_.load_functions(func_exclude_config);

    for(std::size_t i = 0;
        (i < sizeof(default_call_stack_sections) / sizeof(char*));
        ++i)
    {
        sections_exclude_config.add("sections.exclusions.exclude", default_call_stack_sections[i]);
    }

    config_.load_sections(sections_exclude_config);
}

void GuestReplayExecutor::process_function_entries()
{
    using namespace std;

    ELFReader reader(binary());

    vector<Entry> entries;

    entries = reader.get_section_entries(".symtab");

    process_executable_function_entries(entries);

    ProcReader pr(PROC_MAPS_FILE_NAME);

    const vector<ProcMap> proc_maps = pr.find_all();

    set<string> lib_paths;
    for(vector<ProcMap>::const_iterator it = proc_maps.begin();
        it != proc_maps.end();
        ++it)
    {
        if(fs::exists(it->path()))
        {
            lib_paths.insert(it->path());
        }
    }

    for(set<string>::const_iterator it = lib_paths.begin();
        it != lib_paths.end();
        ++it)
    {
        string path = *it;

        vector<ProcMap> pms = pr.find(path);

        ELFReader ler(path);

        uint64_t base_addr = pms.front().address().first;

        vector<Entry> symtab_entries = ler.get_section_entries(".symtab");
        vector<Entry> dynsym_entries = ler.get_section_entries(".dynsym");

        process_library_function_entries(symtab_entries, base_addr, path);
        process_library_function_entries(dynsym_entries, base_addr, path);
    }
}

void GuestReplayExecutor::process_filters()
{
    ELFReader elf_reader(binary());

    process_func_filter(elf_reader,
                        proc_reader_,
                        config_.get_include_functions(),
                        crete_insert_instr_addr_include_filter);
    process_func_filter(elf_reader,
                        proc_reader_,
                        config_.get_exclude_functions(),
                        crete_insert_instr_addr_exclude_filter);

    process_lib_filter(proc_reader_,
                       config_.get_libraries(),
                       crete_insert_instr_addr_include_filter);

    process_executable_section(elf_reader,
                    config_.get_section_exclusions(),
                    crete_insert_instr_call_stack_exclude);

    process_call_stack_library_exclusions(elf_reader,
                                          proc_reader_);

    process_library_sections();
}

void GuestReplayExecutor::process_executable_function_entries(const std::vector<Entry>& entries)
{
    using namespace std;

    bool main_found = false;
    bool libc_main_found = false;

    for(vector<Entry>::const_iterator it = entries.begin();
        it != entries.end();
        ++it)
    {
        if(it->addr == 0)
        {
            // The symtab may list an external reference with address 0. We don't want this one.
            continue;
        }

        if(it->name == "main")
        {
            crete_insert_instr_main_address(it->addr, it->size);

            main_found = true;
        }

        crete_send_function_entry(it->addr, it->size, it->name.c_str());
    }

    if(!main_found)
    {
        throw runtime_error("could not find symbol entry for 'main'!");
    }
}

void GuestReplayExecutor::process_library_function_entries(const std::vector<Entry>& entries,
                                              uint64_t base_addr,
                                              std::string path)
{
    using namespace std;

#if __x86_64__
    const char* const libc_path = "/lib/x86_64-linux-gnu/libc-2.15.so";
#else
    const char* const libc_path = "/lib/i386-linux-gnu/libc-2.15.so";
#endif // __x86_64__

    for(vector<Entry>::const_iterator it = entries.begin();
        it != entries.end();
        ++it)
    {
        if(it->addr == 0)
        {
            // The symtab may list an external reference with address 0. We don't want this one.
            continue;
        }

        crete_send_function_entry(it->addr + base_addr, it->size, it->name.c_str());

        if(path == libc_path)
        {
            if(it->name == "__libc_start_main")
            {
                crete_insert_instr_libc_start_main_address(it->addr + base_addr, it->size);

                libc_main_found_ = true;
            }
            else if(it->name == "exit")
            {
                crete_insert_instr_libc_exit_address(it->addr + base_addr, it->size);

                libc_exit_found_ = true;
            }
        }
    }
}

void GuestReplayExecutor::process_func_filter(ELFReader& reader,
                                 ProcReader& pr,
                                 const config::Functions& funcs,
                                 void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    for(config::Functions::const_iterator it = funcs.begin();
        it != funcs.end();
        ++it)
    {
        if(it->lib.empty()) // Target: executable.
        {
            Entry entry = reader.get_section_entry(".symtab", it->name.c_str());

            if(entry.addr == 0)
            {
                std::cerr
                     << "[CRETE] Warning - failed to get address of '"
                     << it->name
                     << "' - ensure binary has a symbol table '.symtab'\n";
            }

            f_custom_instr(entry.addr, entry.addr + entry.size);
        }
        else // Target: library.
        {
            fs::path lib = deduce_library(it->lib, pr);
            std::vector<ProcMap> pms = pr.find(lib.generic_string());
            assert(pms.size() != 0);

            ELFReader ler(lib); // TODO: inefficient. Re-reading the ELF for each function entry.

            Entry entry = ler.get_section_entry(".symtab", it->name);

            uint64_t base_addr = pms.front().address().first;
            uint64_t addr = base_addr + entry.addr;

            f_custom_instr(addr, addr + entry.size);
        }
    }
}

void GuestReplayExecutor::process_lib_filter(ProcReader& pr,
                                const std::vector<std::string>& libs,
                                void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    using namespace std;

    for(vector<string>::const_iterator iter = libs.begin();
        iter != libs.end();
        ++iter)
    {
        vector<ProcMap> pms = pr.find(*iter);

        if(pms.empty())
            throw runtime_error("failed to get address of '" + *iter + "' library for filtering");

        for(vector<ProcMap>::iterator pmiter = pms.begin();
            pmiter != pms.end();
            ++pmiter)
        {
            f_custom_instr(pmiter->address().first, pmiter->address().second);
        }
    }
}

void GuestReplayExecutor::process_executable_section(ELFReader& reader,
                             const std::vector<std::string>& sections,
                             void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    for(std::vector<std::string>::const_iterator iter = sections.begin();
        iter != sections.end();
        ++iter)
    {
        Entry entry = reader.get_section(*iter);

        if(entry.addr == 0)
            continue;//throw std::runtime_error("failed to get address of '" + *iter + "' - ensure binary has section");

        f_custom_instr(entry.addr, entry.addr + entry.size);
    }
}

void GuestReplayExecutor::process_call_stack_library_exclusions(ELFReader& er,
                                                   const ProcReader& pr)
{
    std::vector<fs::path> libs;

    if(er.get_machine() == EM_386)
    {
        libs.push_back("/lib/ld-linux.so.2");
        libs.push_back("/lib/i386-linux-gnu/libpthread-2.15.so");
    }
    else if(er.get_machine() == EM_X86_64)
    {
        libs.push_back("/lib/x86_64-linux-gnu/ld-2.15.so");
        libs.push_back("/lib/x86_64-linux-gnu/libpthread-2.15.so");
    }
    else
    {
        throw std::runtime_error("Incompatible executable type. Must be i386 or x86_64");
    }

    process_call_stack_library_exclusions(er,
                                          pr,
                                          libs);
}

void GuestReplayExecutor::process_call_stack_library_exclusions(ELFReader& er,
                                                   const ProcReader& pr,
                                                   const std::vector<fs::path>& libs)
{

    for(std::vector<fs::path>::const_iterator it = libs.begin();
        it != libs.end();
        ++it)
    {
        fs::path lib = *it;

        if(fs::is_symlink(lib))
        {
            lib = fs::canonical(lib);
        }

        if(!fs::exists(lib))
        {
            throw std::runtime_error(lib.string() + " not found!");
        }

        std::vector<ProcMap> pms = pr.find(lib.string());

        if(pms.empty())
        {
            throw std::runtime_error(lib.string() + " exists on disk, but not found in proc-maps!");
        }

        uint64_t addr_begin = pms.front().address().first;
        uint64_t addr_end = pms.back().address().second;

        crete_insert_instr_call_stack_exclude(addr_begin, addr_end);
    }
}

void GuestReplayExecutor::process_library_sections()
{
    using namespace std;

    ProcReader pr(PROC_MAPS_FILE_NAME);

    const vector<ProcMap> proc_maps = pr.find_all();

    set<string> lib_paths;
    for(vector<ProcMap>::const_iterator it = proc_maps.begin();
        it != proc_maps.end();
        ++it)
    {
        if(fs::exists(it->path()))
        {
            lib_paths.insert(it->path());
        }
    }

    for(set<string>::const_iterator it = lib_paths.begin();
        it != lib_paths.end();
        ++it)
    {
        string path = *it;

        vector<ProcMap> pms = pr.find(path);

        ELFReader ereader(path);

        uint64_t base_addr = pms.front().address().first;

        process_library_section(ereader,
                                config_.get_section_exclusions(),
                                crete_insert_instr_call_stack_exclude,
                                base_addr);
    }
}

void GuestReplayExecutor::process_library_section(ELFReader& reader,
                                     const std::vector<std::string>& sections,
                                     void (*f_custom_instr)(uintptr_t, uintptr_t),
                                     uint64_t base_addr)
{
    for(std::vector<std::string>::const_iterator iter = sections.begin();
        iter != sections.end();
        ++iter)
    {
        Entry entry = reader.get_section(*iter);

        if(entry.addr == 0)
            continue;//throw std::runtime_error("failed to get address of '" + *iter + "' - ensure binary has section");

        f_custom_instr(entry.addr + base_addr,
                       entry.addr + base_addr + entry.size);
    }
}

// TODO: belongs as a free function in a utility library.
boost::filesystem::path GuestReplayExecutor::deduce_library(const boost::filesystem::path& lib,
                                               const ProcReader& pr)
{
    std::vector<ProcMap> pms = pr.find_all();

    boost::filesystem::path match;

    for(std::vector<ProcMap>::const_iterator it = pms.begin();
        it != pms.end();
        ++it)
    {
        fs::path test_path = it->path();
        if(lib == test_path)
        {
            return test_path;
        }
        else if(test_path.filename() == lib)
        {
            if(!match.empty() && match != test_path) // Same name, but different absolute path.
            {
                throw std::runtime_error("ambiguous libraries found for: " + lib.generic_string());
            }

            match = test_path;
        }
    }

    if(match.empty())
    {
        throw std::runtime_error("unable to find or deduce library: " + lib.generic_string());
    }

    return match;
}

void GuestReplayExecutor::validate_final_checks() const
{
    if(!libc_main_found_)
    {
        throw std::runtime_error("failed to find '__libc_start_main' symbol in libc.so");
    }
}

// -------------------------------------------------------------------------

GuestReplay::GuestReplay(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

void GuestReplay::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

po::options_description GuestReplay::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("config,c", po::value<fs::path>(), "configuration file (found in guest-data/)")
        ("exec,e", po::value<fs::path>(), "executable to test")
        ("gen-html,g", "generate an html report")
        ("reset,r", po::value<fs::path>(), "[unimplemented] clears all previous execution data")
        ("tc-dir,t", po::value<fs::path>(), "test case directory")
        ("trace-mode,m", "use tracing mode")
        ;

    return desc;
}

void GuestReplay::process_options()
{
    bool trace_mode = false;

    if(var_map_.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;
        exit(0);
    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;
        exit(0);
    }
    if(var_map_.count("exec"))
    {
        exec_ = var_map_["exec"].as<fs::path>();
        if(!fs::exists(exec_))
            throw std::runtime_error("[exec] executable not found: " + exec_.generic_string());
    }
    if(var_map_.count("trace-mode"))
    {
        trace_mode = true;
    }
    if(var_map_.count("tc-dir"))
    {
        tc_dir_ = var_map_["tc-dir"].as<fs::path>();
        if(!fs::exists(tc_dir_))
        throw std::runtime_error("[tc-dir] input directory not found: " + tc_dir_.generic_string());
    }
    else if(!exec_.empty())
    {
        throw std::runtime_error("required: option [tc-dir]. See '--help' for more info");
    }
    if(var_map_.count("config"))
    {
        config_ = var_map_["config"].as<fs::path>();
        if(!fs::exists(config_))
        {
            throw std::runtime_error("[config] file not found: " + config_.string());
        }
    }
    else if(!exec_.empty())
    {
            throw std::runtime_error("required: option [config]. See '--help' for more info");
    }

    if(!exec_.empty())
        std::runtime_error("required: option [exec]. See '--help' for more info");

#if defined(HOST_DRIVEN) || 1
    char cprog[56];

    crete_insert_instr_next_replay_program((uintptr_t)&cprog[0], (uintptr_t)sizeof(cprog));

    std::string sprog = cprog;

    exec_ = exec_.parent_path() / sprog;
    tc_dir_ = tc_dir_.parent_path() / sprog;

#endif // defined(HOST_DRIVEN) || 1

    GuestReplayExecutor executor(exec_,
                                 tc_dir_,
                                 config_,
                                 trace_mode);

    executor.execute_all();

    auto working_dir = fs::current_path();
}

} // namespace crete

int main(int argc, char* argv[])
{
    try
    {
        crete::GuestReplay{argc, argv};
    }
    catch(std::exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
        return -1;
    }

    return 0;
}
