#include "runner.h"

#include <crete/run_config.h>
#include <crete/custom_instr.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/client.h>

#include <boost/process.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace pt = boost::property_tree;
namespace ba = boost::algorithm;
namespace po = boost::program_options;
using namespace msm::front;

namespace crete
{

// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
//    struct error {};
    struct next_test {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct poll {};
struct next_test{};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class RunnerFSM_ : public boost::msm::front::state_machine_def<RunnerFSM_>
{
private:
    std::string host_ip_;
    std::string host_port_;
    boost::shared_ptr<Client> client_;
    fs::path guest_config_path_;
    config::RunConfiguration guest_config_;
    pid_t pid_;

    bool libc_main_found_;
    bool libc_exit_found_;
    bool is_first_exec_;
    std::size_t proc_maps_hash_;

public:
    RunnerFSM_();

    void prime_virtual_machine();
    void prime_harness();
    void write_configuration() const;
    void launch_executable();
    void signal_dump() const;

    void process_func_filter(ELFReader& reader,
                             ProcReader& pr,
                             const config::Functions& funcs,
                             void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_lib_filter(ProcReader& pr,
                            const std::vector<std::string>& libs,
                            void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_executable_section(ELFReader& reader,
                                    const std::vector<std::string>& sections,
                                    void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_call_stack_library_exclusions(ELFReader& er,
                                               const ProcReader& pr);
    void process_call_stack_library_exclusions(const ProcReader& pr,
                                               const std::vector<boost::filesystem::path>& libraries);
    void process_library_sections(const ProcReader& pr);
    void process_library_section(ELFReader& reader,
                                 const std::vector<std::string>& sections,
                                 void (*f_custom_instr)(uintptr_t, uintptr_t),
                                 uint64_t base_addr);
    void process_function_entries(ELFReader& reader,
                                  const ProcReader& pr);
    void process_executable_function_entries(const std::vector<Entry>& entries);
    void process_library_function_entries(const std::vector<Entry>& entries,
                                          uint64_t base_addr,
                                          std::string path);
    fs::path deduce_library(const fs::path& lib,
                            const ProcReader& pr);



public:
    // +--------------------------------------------------+
    // FSM                                                +
    // +--------------------------------------------------+

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
        std::cout << "entering: RunnerFSM_" << std::endl;
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
        std::cout << "leaving: RunnerFSM_" << std::endl;
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
    };
    struct VerifyEnv : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: VerifyEnv" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: VerifyEnv" << std::endl;}
    };
    struct Clean : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: Clean" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: Clean" << std::endl;}
    };
    struct ConnectHost : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: ConnectHost" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: ConnectHost" << std::endl;}
    };
    struct LoadHostData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: LoadHostData" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: LoadHostData" << std::endl;}
    };
    struct LoadDefaults : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: LoadDefaults" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: LoadDefaults" << std::endl;}
    };
    struct LoadInputData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: LoadInitialInputs" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: LoadInitialInputs" << std::endl;}
    };
    struct TransmitGuestData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: TransmitGuestData" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: TransmitGuestData" << std::endl;}
    };
    struct Prime : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: Prime" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: Prime" << std::endl;}
    };
    struct ProcessConfig : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: ProcessConfig" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: ProcessConfig" << std::endl;}
    };
    struct ValidateSetup : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: ValidateSetup" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: ValidateSetup" << std::endl;}
    };
    struct UpdateConfig : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: WriteConfig" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: WriteConfig" << std::endl;}
    };
    struct Execute : public msm::front::state<>
    {
        typedef mpl::vector1<flag::next_test> flag_list;

        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: Execute" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: Execute" << std::endl;}
    };
    struct Finished : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: Finished" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: Finished" << std::endl;}
    };
    struct VerifyInvariants : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cout << "entering: VerifyInvariants" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cout << "leaving: VerifyInvariants" << std::endl;}
    };

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    void init(const start&);
    void verify_env(const poll&);
    void clean(const poll&);
    void connect_host(const poll&);
    void load_host_data(const poll&);
    void load_defaults(const poll&);
    void load_input_data(const poll&);
    void transmit_guest_data(const poll&);
    void prime(const poll&);
    void process_config(const poll&);
    void validate_setup(const poll&);
    void update_config(const poll&);
    void execute(const next_test&);
    void finished(const poll&);
    void verify_invariants(const poll&);

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+

    bool is_process_finished(const poll&);
    bool is_first_exec(const poll&);

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught (Event const&,FSM& fsm,std::exception& e)
    {
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "RunnerFSM_: exception thrown from within FSM");
    }

    typedef RunnerFSM_ M;

    // Initial state of the FSM.
    typedef Start initial_state;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Start             ,start             ,VerifyEnv         ,&M::init                                  >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<VerifyEnv         ,poll              ,Clean             ,&M::verify_env                            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Clean             ,poll              ,ConnectHost       ,&M::clean                                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ConnectHost       ,poll              ,LoadHostData      ,&M::connect_host                          >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadHostData      ,poll              ,LoadDefaults      ,&M::load_host_data                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadDefaults      ,poll              ,LoadInputData     ,&M::load_defaults                         >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadInputData     ,poll              ,TransmitGuestData ,&M::load_input_data                       >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<TransmitGuestData ,poll              ,Prime             ,&M::transmit_guest_data                   >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Prime             ,poll              ,ProcessConfig     ,&M::prime                                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ProcessConfig     ,poll              ,ValidateSetup     ,&M::process_config                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ValidateSetup     ,poll              ,Execute           ,&M::validate_setup                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Execute           ,next_test         ,Finished          ,&M::execute                               >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      row<Finished          ,poll              ,VerifyInvariants  ,&M::finished         ,&M::is_process_finished >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      row<VerifyInvariants  ,poll              ,UpdateConfig      ,&M::verify_invariants,&M::is_first_exec   >,
    a_row<VerifyInvariants  ,poll              ,Execute           ,&M::verify_invariants                     >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<UpdateConfig      ,poll              ,Execute           ,&M::update_config                          >
    > {};
};

// Normally would just: "using RunnerFSM = boost::msm::back::state_machine<RunnerFSM>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class RunnerFSM : public boost::msm::back::state_machine<RunnerFSM_>
{
};

struct start // Basically, serves as constructor.
{
    start(const std::string& host_ip,
          const fs::path& config) :
        host_ip_(host_ip),
        config_(config)
    {}

    const std::string& host_ip_;
    const fs::path& config_;
};

RunnerFSM_::RunnerFSM_() :
    client_(),
    pid_(-1),
    libc_main_found_(false),
    libc_exit_found_(false),
    is_first_exec_(true),
    proc_maps_hash_(0)
{
}

void RunnerFSM_::init(const start& ev)
{
    host_ip_ = ev.host_ip_;
    guest_config_path_ = ev.config_;
}

void RunnerFSM_::verify_env(const poll&)
{
    // TODO: LD_BIND_NOW is not required unless using call-stack monitoring.

    const char* env = std::getenv("LD_BIND_NOW");

    if(env == NULL || std::string("1") != env)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::invalid_env("LD_BIND_NOW environmental variable not set to 1"));
    }
}

void RunnerFSM_::clean(const poll&)
{
    fs::remove(log_file_name);
    fs::remove(proc_maps_file_name);
}

void RunnerFSM_::connect_host(const poll&)
{
    std::cout << "[CRETE] Waiting for port..." << std::endl;

    Port port = 0;

    do
    {
        crete_insert_instr_read_port((uintptr_t)&port, (uintptr_t)sizeof(port));

    }while(port == 0);

    std::cout << "[CRETE] Connecting to host '"
              << host_ip_
              << "' on port "
              << port
              << "' ..."
              << std::endl;

    client_ = boost::make_shared<Client>(host_ip_,
                                         boost::lexical_cast<std::string>(port));

    client_->connect();
}

void RunnerFSM_::load_host_data(const poll&)
{
    bool distributed = false;

    read_serialized_text(*client_,
                         distributed);

    if(distributed)
    {
        if(!guest_config_path_.empty())
        {
            BOOST_THROW_EXCEPTION(Exception() << err::mode("target configuration file provided while in 'distributed' mode")
                                              << err::msg("please use omit the argument, or use 'developer' mode"));
        }

        std::string tmp;

        read_serialized_text(*client_,
                             tmp);

        guest_config_path_ = tmp;
    }
    else if(guest_config_path_.empty())
    {
        BOOST_THROW_EXCEPTION(Exception() << err::mode("target configuration file NOT provided while in 'developer' mode")
                                          << err::msg("please provide the argument, or use 'distributed' mode"));
    }
}

void RunnerFSM_::load_defaults(const poll&)
{
    try
    {
        pt::ptree ptree;

        pt::read_xml(guest_config_path_.string(),
                     ptree);

        guest_config_ = config::RunConfiguration(ptree);
    }
    catch(pt::file_parser_error& e)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::parse(guest_config_path_.string())
                                          << err::msg(e.what()));
    }

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

    const char* const vm_comm_lib = "libcrete_vm_comm.so";
    const char* const preload_lib = "libcrete_preload.so";
    const char* const hook_lib = "libcrete_hook.so";

    pt::ptree preloads_config;
    pt::ptree libs_config;
    pt::ptree func_include_config;
    pt::ptree func_exclude_config;
    pt::ptree sections_exclude_config;

    {
        pt::ptree& node = preloads_config.add_child("preloads.preload", pt::ptree());
        node.put("<xmlattr>.path", preload_lib);
        if(guest_config_.get_files().size() > 0)
        {
            pt::ptree& nodeh = preloads_config.add_child("preloads.preload", pt::ptree());
            nodeh.put("<xmlattr>.path", hook_lib);

            default_libraries.push_back(hook_lib);
        }
    }

    guest_config_.load_preloads(preloads_config);

    for(std::vector<std::string>::const_iterator it = default_libraries.begin();
        it != default_libraries.end();
        ++it)
    {
        pt::ptree& node = libs_config.add_child("libs.lib", pt::ptree());
        node.put("<xmlattr>.path", *it);
    }

    guest_config_.load_libraries(libs_config);

    for(std::size_t i = 0;
        (i < sizeof(default_function_includes) / sizeof(char*));
        ++i)
    {
        const char* name = default_function_includes[i];

        pt::ptree& node = func_include_config.add_child("funcs.include.func", pt::ptree());
        node.put("<xmlattr>.name", name);
        node.put("<xmlattr>.lib", vm_comm_lib);
    }

    guest_config_.load_functions(func_include_config);

    for(std::size_t i = 0;
        (i < sizeof(default_function_excludes) / sizeof(char*));
        ++i)
    {
        const char* name = default_function_excludes[i];

        pt::ptree& node = func_exclude_config.add_child("funcs.exclude.func", pt::ptree());
        node.put("<xmlattr>.name", name);
        node.put("<xmlattr>.lib", vm_comm_lib);
    }

    guest_config_.load_functions(func_exclude_config);

    for(std::size_t i = 0;
        (i < sizeof(default_call_stack_sections) / sizeof(char*));
        ++i)
    {
        sections_exclude_config.add("sections.exclusions.exclude", default_call_stack_sections[i]);
    }

    guest_config_.load_sections(sections_exclude_config);
}

void RunnerFSM_::load_input_data(const poll&)
{
    guest_config_.load_file_data();
}

void RunnerFSM_::transmit_guest_data(const poll&)
{
    PacketInfo pk;
    pk.id = 0; // TODO: Don't care? Maybe. What about a custom instruction that reads the VM's pid as an ID, to be checked both for sanity and to give the instance a way to check whether the VM is still running.
    pk.size = 0; // Don't care. Set by write_serialized_text.
    pk.type = packet_type::guest_configuration;
    write_serialized_text_xml(*client_,
                              pk,
                              guest_config_);
}

void RunnerFSM_::prime(const poll&)
{
    prime_virtual_machine();
    prime_harness();
}

void RunnerFSM_::prime_virtual_machine()
{
#if !defined(CRETE_TEST)

    crete_send_custom_instr_prime();
    crete_insert_instr_call_stack_size(guest_config_.get_exploration().call_depth);
    crete_insert_instr_stack_depth_bounds(guest_config_.get_exploration().stack_depth.low,
                                          guest_config_.get_exploration().stack_depth.high);
#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::prime_harness()
{
    guest_config_.is_first_iteration(true);
    write_configuration();

#if !defined(CRETE_TEST)

    launch_executable();

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::write_configuration() const
{
    std::ofstream ofs(harness_config_file_name.c_str());

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(harness_config_file_name));
    }

    boost::archive::text_oarchive oa(ofs);
    oa << guest_config_;
}

void RunnerFSM_::launch_executable()
{
    std::string joined_preloads;

    config::Preloads preloads = guest_config_.get_preloads();

    for(config::Preloads::const_iterator it = preloads.begin();
        it != preloads.end();
        ++it)
    {
        joined_preloads += it->lib.string();
        joined_preloads += " ";
    }

    bp::context ctx;
    ctx.stdout_behavior = bp::capture_stream();
    ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    ctx.work_directory = fs::current_path().string();
    ctx.environment = bp::self::get_environment();
    ctx.environment.erase("LD_PRELOAD");
    ctx.environment.insert(bp::environment::value_type( "LD_PRELOAD"
                                                      , joined_preloads));
    ctx.environment.erase("LD_LIBRARY_PATH");
    ctx.environment.insert(bp::environment::value_type( "LD_LIBRARY_PATH"
                                                      , std::getenv("LD_LIBRARY_PATH")));

    std::cerr << "LD_PRELOAD: " << joined_preloads << std::endl;
    std::cerr << "exe: " << guest_config_.get_executable().string() << std::endl;

    fs::path exe = guest_config_.get_executable();
    std::vector<std::string> args;
    args.push_back(exe.filename().string());

    bp::child proc = bp::launch(exe.string(),
                                args,
                                ctx);

    bp::pistream& is = proc.get_stdout();
    std::string line;
    while(std::getline(is, line))
        std::cout << line << std::endl;

    pid_ = proc.get_id();
    bp::status s = proc.wait();

    // TODO: what I should be doing is storing the bp::child, so once it's finished,
    // I can check the exit status. Do I really care about the exit status?

//    if(s.exit_status() != 0)
//    {
//        // TODO: exception, or error state?
//        BOOST_THROW_EXCEPTION(Exception() << err::process_exit_status(exe.string()));
//    }
}

void RunnerFSM_::signal_dump() const
{
#if !defined(CRETE_TEST)

    bp::context ctx;
    ctx.work_directory = fs::current_path().string();
    ctx.environment = bp::self::get_environment();

    fs::path exe = bp::find_executable_in_path("crete-dump");
    std::vector<std::string> args;
    args.push_back(exe.filename().string());

    bp::child proc = bp::launch(exe.string(),
                                args,
                                ctx);

    bp::status s = proc.wait();

    if(s.exit_status() != 0)
    {
        // TODO: exception, or error state?
        BOOST_THROW_EXCEPTION(Exception() << err::process_exit_status(exe.string()));
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_config(const poll&)
{
#if !defined(CRETE_TEST)

    using namespace std;

    ELFReader elf_reader(guest_config_.get_executable());
    ProcReader proc_reader(proc_maps_file_name);

    process_func_filter(elf_reader,
                        proc_reader,
                        guest_config_.get_include_functions(),
                        crete_insert_instr_addr_include_filter);
    process_func_filter(elf_reader,
                        proc_reader,
                        guest_config_.get_exclude_functions(),
                        crete_insert_instr_addr_exclude_filter);

    process_lib_filter(proc_reader,
                       guest_config_.get_libraries(),
                       crete_insert_instr_addr_include_filter);

    process_executable_section(elf_reader,
                               guest_config_.get_section_exclusions(),
                               crete_insert_instr_call_stack_exclude);

    process_call_stack_library_exclusions(elf_reader,
                                          proc_reader);

    process_library_sections(proc_reader);

    process_function_entries(elf_reader,
                             proc_reader);

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::validate_setup(const poll&)
{
#if !defined(CRETE_TEST)

    if(!libc_main_found_)
    {
        throw std::runtime_error("failed to find '__libc_start_main' symbol in libc.so");
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::update_config(const poll&)
{
    guest_config_.is_first_iteration(false);
    guest_config_.clear_file_data();

    write_configuration();

    is_first_exec_ = false;
}

void RunnerFSM_::execute(const next_test&)
{
    // TODO: should waiting for the command to come in be a guard?
    PacketInfo pkinfo = client_->read();

    if(pkinfo.type != packet_type::cluster_next_test)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::network_type_mismatch(pkinfo.type));
    }

#if !defined(CRETE_TEST)

    launch_executable();

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::finished(const poll&)
{
    signal_dump();

    pid_ = -1;
}

void RunnerFSM_::verify_invariants(const poll&)
{
#if !defined(CRETE_TEST)

    std::ifstream ifs(proc_maps_file_name.c_str());
    std::string contents((
        std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());

    boost::hash<std::string> hash_fn;

    std::size_t new_hash = hash_fn(contents);

    assert(new_hash);

    if(proc_maps_hash_ == 0)
    {
        proc_maps_hash_ = new_hash;
    }
    else if(new_hash != proc_maps_hash_)
    {
        throw std::runtime_error("proc-maps.log changed across iterations! Ensure ASLR is disabled");
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_library_function_entries(const std::vector<Entry>& entries,
                                                  uint64_t base_addr,
                                                  std::string path)
{
#if !defined(CRETE_TEST)

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

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_function_entries(ELFReader& reader,
                                          const ProcReader& pr)
{
    using namespace std;

    vector<Entry> entries;

    entries = reader.get_section_entries(".symtab");

    process_executable_function_entries(entries);

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

void RunnerFSM_::process_executable_function_entries(const std::vector<Entry>& entries)
{
#if !defined(CRETE_TEST)

    using namespace std;

    bool main_found = false;

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

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_call_stack_library_exclusions(ELFReader& er,
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

    process_call_stack_library_exclusions(pr,
                                          libs);
}

void RunnerFSM_::process_call_stack_library_exclusions(const ProcReader& pr,
                                                       const std::vector<fs::path>& libs)
{
#if !defined(CRETE_TEST)

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
            // TODO: print warning?
//            throw std::runtime_error(lib.string() + " exists on disk, but not found in proc-maps!");
            continue;
        }

        uint64_t addr_begin = pms.front().address().first;
        uint64_t addr_end = pms.back().address().second;

        crete_insert_instr_call_stack_exclude(addr_begin, addr_end);
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_func_filter(ELFReader& reader,
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

// TODO: belongs as a free function in a utility library.
fs::path RunnerFSM_::deduce_library(const fs::path& lib,
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

void RunnerFSM_::process_lib_filter(ProcReader& pr,
                                const std::vector<std::string>& libs,
                                void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    using namespace std;

    for(vector<string>::const_iterator iter = libs.begin();
        iter != libs.end();
        ++iter)
    {
        fs::path lib = deduce_library(*iter, pr);
        vector<ProcMap> pms = pr.find(lib.string());

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

void RunnerFSM_::process_executable_section(ELFReader& reader,
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

void RunnerFSM_::process_library_sections(const ProcReader& pr)
{
#if !defined(CRETE_TEST)

    using namespace std;

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
                                guest_config_.get_section_exclusions(),
                                crete_insert_instr_call_stack_exclude,
                                base_addr);
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_library_section(ELFReader& reader,
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

bool RunnerFSM_::is_process_finished(const poll&)
{
    return !process::is_running(pid_);
}

bool RunnerFSM_::is_first_exec(const poll&)
{
    return is_first_exec_;
}

Runner::Runner(int argc, char* argv[]) :
    ops_descr_(make_options()),
    fsm_(boost::make_shared<RunnerFSM>()),
    stopped_(false)
{
    parse_options(argc, argv);
    process_options();

    start_FSM();

    run();
}

po::options_description Runner::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
            ("help,h", "displays help message")
            ("config,c", po::value<fs::path>(), "configuration file")
            ("ip,i", po::value<std::string>(), "host IP")
        ;

    return desc;
}

void Runner::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

void Runner::process_options()
{
    using namespace std;

//    if(var_map_.size() == 0)
//    {
//        cout << "Missing arguments" << endl;
//        cout << "Use '--help' for more details" << endl;
        
//        BOOST_THROW_EXCEPTION(Exception() << err::arg_count(0));
//    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;

        throw Exception();
    }
    if(var_map_.count("ip"))
    {
        ip_ = var_map_["ip"].as<std::string>();
    }
    else // Default IP
    {
        ip_ = "10.0.2.2";
    }
    if(var_map_.count("config"))
    {
        fs::path p = var_map_["config"].as<fs::path>();

        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        target_config_ = p;
    }
}

void Runner::start_FSM()
{
    fsm_->start();
}

void Runner::stop()
{
    stopped_ = true;
}

void Runner::run()
{
    start s(ip_,
            target_config_);

    fsm_->process_event(s);

    while(!stopped_)
    {
        if(fsm_->is_flag_active<flag::next_test>())
        {
            fsm_->process_event(next_test());
        }
        else
        {
            fsm_->process_event(poll());
        }
    }
}

} // namespace crete
