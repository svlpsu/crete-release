#include <crete/cluster/svm_node_fsm.h>
#include <crete/cluster/common.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/cluster/svm_node_options.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/server.h>
#include <crete/run_config.h>
#include <crete/serialize.h>
#include <crete/async_task.h>
#include <crete/logger.h>
#include <crete/util/debug.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/range/algorithm/replace_if.hpp>

#include <boost/process.hpp>

#include <memory>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace pt = boost::property_tree;
namespace bui = boost::uuids;
using namespace msm::front;

namespace crete
{
namespace cluster
{
namespace node
{
namespace svm
{

const auto klee_dir_name = std::string{"klee-run"};
const auto concolic_log_name = std::string{"concolic.log"};
const auto symbolic_log_name = std::string{"klee-run.log"};

// +--------------------------------------------------+
// + Exceptions                                       +
// +--------------------------------------------------+
struct SVMException : public Exception {};
struct ConcolicExecException : public SVMException {};
struct SymbolicExecException : public SVMException
{
    SymbolicExecException(const std::vector<TestCase>& tests)
        : tests_{tests}
    {}

    std::vector<TestCase> tests_;
};

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+

namespace ev
{

struct start // Basically, serves as constructor.
{
    start(const std::string& svm_dir
         ,const cluster::option::Dispatch& dispatch_options
         ,node::option::SVMNode& node_options)
        : svm_dir_{svm_dir}
        , dispatch_options_(dispatch_options)
        , node_options_{node_options} {}

    std::string svm_dir_;
    cluster::option::Dispatch dispatch_options_;
    node::option::SVMNode node_options_;
};

struct next_trace
{
    next_trace(const Trace& trace) :
        trace_(trace)
    {}

    const Trace& trace_;
};

struct connect
{
    connect(Port master_port) :
        master_port_(master_port)
    {}

    Port master_port_;
};

} // namespace ev

namespace fsm
{
auto retrieve_tests(const fs::path& kdir) -> std::vector<TestCase>;

// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class KleeFSM_ : public boost::msm::front::state_machine_def<KleeFSM_>
{
public:
    KleeFSM_();

    auto tests() -> std::vector<TestCase>;
    auto error() -> const log::NodeError&;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
        std::cout << "entering: KleeFSM_" << std::endl;
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
        std::cout << "leaving: KleeFSM_" << std::endl;
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct NextTrace;
    struct Prepare;
    struct Prepared;
    struct ExecuteConcolic;
    struct ExecuteSymbolic;
    struct StoreTests;
    struct Finished;
    struct ResultReady;

    struct Valid;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct next_trace;
    struct prepare;
    struct execute_concolic;
    struct execute_symbolic;
    struct retrieve_result;
    struct clean;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_prev_task_finished;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+
    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e);

    // Initial state of the FSM.
    using initial_state = mpl::vector<Start, Valid>;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,ev::start         ,NextTrace         ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<NextTrace         ,ev::next_trace    ,Prepare           ,next_trace           ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Prepare           ,ev::poll          ,ExecuteConcolic   ,prepare              ,is_prev_task_finished>,
//      Row<Prepare           ,ev::poll          ,ExecuteSymbolic   ,prepare              ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ExecuteConcolic   ,ev::poll          ,ExecuteSymbolic   ,execute_concolic     ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ExecuteSymbolic   ,ev::poll          ,StoreTests        ,execute_symbolic     ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StoreTests        ,ev::poll          ,Finished          ,retrieve_result      ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Finished          ,ev::poll          ,ResultReady       ,none                 ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ResultReady       ,ev::tests_queued  ,NextTrace         ,none/*clean*/                ,none                 >,
    // -- Orthogonal Region
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Valid             ,ev::error         ,Error             ,none                 ,none                 >
    > {};

private:
    cluster::option::Dispatch dispatch_options_;
    node::option::SVMNode node_options_;
    fs::path svm_dir_;
    fs::path trace_dir_;
    std::shared_ptr<std::vector<TestCase>> tests_ = std::make_shared<std::vector<TestCase>>();
    crete::log::Logger exception_log_;
    log::NodeError error_log_;
};

template <class FSM,class Event>
void KleeFSM_::exception_caught(Event const&,FSM& fsm,std::exception& e)
{
    auto except_info = boost::diagnostic_information(e);
    std::cerr << "exception_caught" << std::endl;
    std::cerr << except_info << std::endl;
    exception_log_ << except_info;

    std::stringstream ss;

    ss << "Exception Caught:\n"
       << except_info
       << "Node: SVM\n"
       << "Trace: " << fsm.trace_dir_ << "\n"
       << "\n";
    // TODO: I'd also like to send the current state's ID (ideally, even the recent state history), but that requires some work: http://stackoverflow.com/questions/14166800/boostmsm-a-way-to-get-a-string-representation-ie-getname-of-a-state
    // Cont: I could ad hoc it by keeping a history_ variable that I append to for each state, and condense to the last N.
    // Cont: history_ could be stored using boost::circular_buffer, or std::vector with a mod operator, to keep it's size down to N.


    if(dynamic_cast<SVMException*>(&e))
    {
        auto dump_log_file = [](std::stringstream& ss,
                                const fs::path& log_file)
        {
            fs::ifstream ifs(log_file);

            if(!ifs.good())
            {
                ss << "Failed to open associated log file: "
                   << concolic_log_name;
            }
            else
            {
                ss << "Log File ["
                   << concolic_log_name
                   << "]:\n"
                   << ifs.rdbuf()
                   << '\n';
            }
        };

        if(dynamic_cast<ConcolicExecException*>(&e))
        {
            dump_log_file(ss, fsm.trace_dir_ / klee_dir_name / concolic_log_name);
        }
        else if(dynamic_cast<SymbolicExecException*>(&e))
        {
            auto* se = dynamic_cast<SymbolicExecException*>(&e);

            *fsm.tests_ = se->tests_;

            dump_log_file(ss, fsm.trace_dir_ / klee_dir_name / symbolic_log_name);
        }

        fsm.process_event(ev::error{});
    }
    else
    {
        // TODO: handle more gracefully...
        assert(0 && "Unknown exception");
//        fsm.process_event(ev::terminate{});
    }

    error_log_.log = ss.str();
}

inline
auto retrieve_tests(const fs::path& kdir) -> std::vector<TestCase>
{
    auto test_pool_dir = kdir / "ktest_pool";

    auto tests = std::vector<TestCase>{};

    if(!fs::exists(test_pool_dir))
    {
        BOOST_THROW_EXCEPTION(SVMException{} << err::file_missing{test_pool_dir.string()});
    }

    fs::directory_iterator end;
    for(fs::directory_iterator iter{test_pool_dir}; iter != end; ++iter)
    {
        auto entry = *iter;

        if(fs::file_size(entry) == 4) // If test case file generated, but no elements.
        {
            BOOST_THROW_EXCEPTION(SVMException{} << err::file{entry.path().string()});
        }

        fs::ifstream tests_file(entry);

        tests.emplace_back(read_test_case(tests_file));
    }

    return tests;
}

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
inline
KleeFSM_::KleeFSM_()
{
}

inline
auto KleeFSM_::tests() -> std::vector<TestCase>
{
    return *tests_;
}

inline
auto KleeFSM_::error() -> const log::NodeError&
{
    return error_log_;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+
struct KleeFSM_::Start : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
};
struct KleeFSM_::NextTrace : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::next_trace>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: NextTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: NextTrace" << std::endl;}
};
struct KleeFSM_::Prepare : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Prepare" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Prepare" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::ExecuteConcolic : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ExecuteConcolic" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ExecuteConcolic" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::ExecuteSymbolic : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ExecuteSymbolic" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ExecuteSymbolic" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::StoreTests : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: StoreTests" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: StoreTests" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::Finished : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Finished" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Finished" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::ResultReady : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tests_ready>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ResultReady" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ResultReady" << std::endl;}
};
struct KleeFSM_::Valid : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Valid" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Valid" << std::endl;}
};
struct KleeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+
struct KleeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.svm_dir_ = ev.svm_dir_;
        fsm.dispatch_options_ = ev.dispatch_options_;
        fsm.node_options_ = ev.node_options_;

        if(!fs::exists(fsm.svm_dir_))
        {
            fs::create_directories(fsm.svm_dir_);
        }

        fsm.exception_log_.add_sink(fsm.svm_dir_ / log_dir_name / exception_log_file_name);
        fsm.exception_log_.auto_flush(true);
    }
};

struct KleeFSM_::next_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        fsm.trace_dir_ = fsm.svm_dir_ / bui::to_string(ev.trace_.uuid_);

        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir,
                                              Trace trace)
        {
            std::cerr << "to_file() started" << std::endl;

            to_file(trace,
                    trace_dir);

            std::cerr << "to_file() finished" << std::endl;

        }, fsm.trace_dir_, ev.trace_}); // TODO: inefficient. Should use shared_ptr probably. Be wary about lifetimes, as these objects will go out of scope and the thread will continue.
    }
};

struct KleeFSM_::prepare
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        std::cerr << "operator() prepare" << std::endl;

        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir
                                             ,cluster::option::Dispatch dispatch_options
                                             ,option::SVMNode node_options)
        {
            fs::path dir = trace_dir;
            fs::path kdir = dir / klee_dir_name;
            auto copy_files = std::vector<std::string>{
                                                       "concrete_inputs.bin",
                                                       "dump_llvm.bc",
                                                       "dump_mo_symbolics.txt",
                                                       "dump_qemu_interrupt_info.bin",
                                                       "dump_sync_memos.bin",
                                                       "dump_tbPrologue_regs.bin",
                                                       "main_function.ll"
                                                      };

            if(!fs::exists(dir))
            {
                BOOST_THROW_EXCEPTION(SVMException{} << err::file_missing{dir.string()});
            }

            bp::context ctx;
            ctx.work_directory = dir.string();
            ctx.environment = bp::self::get_environment();

            {
                auto exe = std::string{};

                if(dispatch_options.vm.arch == "x86")
                {
                    if(!node_options.translator.path.x86.empty())
                    {
                        exe = node_options.translator.path.x86;
                    }
                    else
                    {
                        exe = bp::find_executable_in_path("crete-llvm-translator-i386");
                    }
                }
                else if(dispatch_options.vm.arch == "x64")
                {
                    if(!node_options.translator.path.x64.empty())
                    {
                        exe = node_options.translator.path.x64;
                    }
                    else
                    {
                        exe = bp::find_executable_in_path("crete-llvm-translator-x86_64");
                    }
                }
                else
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{dispatch_options.vm.arch}
                                                      << err::arg_invalid_str{"vm.arch"});
                }

                auto args = std::vector<std::string>{fs::absolute(exe).string()}; // It appears our modified QEMU requires full path in argv[0]...

                auto proc = bp::launch(exe, args, ctx);
                auto status = proc.wait();

                if(!process::is_exit_status_zero(status))
                {
                    // TODO: rdbuf into exception
                    BOOST_THROW_EXCEPTION(SVMException{} << err::process_exit_status{exe});
                }

                fs::rename(dir / "dump_llvm_offline.bc",
                           dir / "dump_llvm.bc");
                fs::remove(dir / "dump_tcg_llvm_offline.bin");
            }

            if(!fs::exists(kdir))
            {
                fs::create_directory(kdir);
            }

            for(const auto f : copy_files)
            {
                fs::copy_file(dir/f, kdir/f);
            }

            ctx.work_directory = kdir.string();

            {
                // TODO: make llvm-as path optional in config?
                auto exe = bp::find_executable_in_path("llvm-as");
                auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                                     "main_function.ll",
                                                     "-o=" + std::string("main_function.bc")};

                auto proc = bp::launch(exe, args, ctx);
                auto status = proc.wait();

                if(!process::is_exit_status_zero(status))
                {
                    BOOST_THROW_EXCEPTION(SVMException{} << err::process_exit_status{exe});
                }
            }

            {
                // TODO: make llvm-link path optional in config?
                auto exe = bp::find_executable_in_path("llvm-link");
                auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                                     "main_function.bc",
                                                     "dump_llvm.bc",
                                                     "-o",
                                                     "run.bc"};

                auto proc = bp::launch(exe, args, ctx);
                auto status = proc.wait();

                if(!process::is_exit_status_zero(status))
                {
                    BOOST_THROW_EXCEPTION(SVMException{} << err::process_exit_status{exe});
                }
            }
        }
        , fsm.trace_dir_
        , fsm.dispatch_options_
        , fsm.node_options_});
    }
};

struct KleeFSM_::execute_concolic
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir
                                             ,cluster::option::Dispatch dispatch_options
                                             ,option::SVMNode node_options)
        {
            auto kdir = trace_dir / klee_dir_name;

            bp::context ctx;
            ctx.work_directory = kdir.string();
            ctx.environment = bp::self::get_environment();
            ctx.stdout_behavior = bp::capture_stream();
            ctx.stderr_behavior = bp::redirect_stream_to_stdout();

            auto exe = std::string{};

            if(!node_options.svm.path.concolic.empty())
            {
                exe = node_options.svm.path.concolic;
            }
            else
            {
                exe = bp::find_executable_in_path("klee");
            }

            auto args = std::vector<std::string>{fs::path{exe}.filename().string()};

            auto add_args = std::vector<std::string>{};

            boost::split(add_args
                        ,dispatch_options.svm.args.concolic
                        ,boost::is_any_of(" \t\n"));

            add_args.erase(std::remove_if(add_args.begin(),
                                          add_args.end(),
                                          [](const std::string& s)
                                          {
                                              return s.empty();
                                          }),
                           add_args.end());

            args.insert(args.end()
                       ,add_args.begin()
                       ,add_args.end());

            args.emplace_back("run.bc");

            for(const auto& e : args)
                std::cerr << "arg: " << e << std::endl;

            auto proc = bp::launch(exe, args, ctx);

            auto log_path = kdir / concolic_log_name;

            {
                fs::ofstream ofs(log_path);
                if(!ofs.good())
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{log_path.string()});
                }

                bp::pistream& is = proc.get_stdout();
                std::string line;
                while(std::getline(is, line))
                {
                    ofs << line << '\n';
                }
            }

            auto status = proc.wait();

            if(!process::is_exit_status_zero(status))
            {
                BOOST_THROW_EXCEPTION(ConcolicExecException{} << err::process_exit_status{exe});
            }

            if(!util::debug::is_last_line_correct(log_path))
            {
                BOOST_THROW_EXCEPTION(ConcolicExecException{} << err::process{exe});
            }
        }
        , fsm.trace_dir_
        , fsm.dispatch_options_
        , fsm.node_options_});
    }
};

struct KleeFSM_::execute_symbolic
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir
                                             ,cluster::option::Dispatch dispatch_options
                                             ,option::SVMNode node_options)
        {
            auto kdir = trace_dir / klee_dir_name;

            bp::context ctx;
            ctx.work_directory = kdir.string();
            ctx.environment = bp::self::get_environment();
            ctx.stdout_behavior = bp::capture_stream();
            ctx.stderr_behavior = bp::redirect_stream_to_stdout();

            auto exe = std::string{};

            if(!node_options.svm.path.symbolic.empty())
            {
                exe = node_options.svm.path.symbolic;
                std::cerr << "symbolic exe: " << exe << std::endl;
            }
            else
            {
                exe = bp::find_executable_in_path("klee");
            }

            auto args = std::vector<std::string>{fs::path{exe}.filename().string()};

            auto add_args = std::vector<std::string>{};

            boost::split(add_args
                        ,dispatch_options.svm.args.symbolic
                        ,boost::is_any_of(" \t\n"));

            add_args.erase(std::remove_if(add_args.begin(),
                                          add_args.end(),
                                          [](const std::string& s)
                                          {
                                              return s.empty();
                                          }),
                           add_args.end());

            args.insert(args.end()
                       ,add_args.begin()
                       ,add_args.end());

            args.emplace_back("run.bc");

            for(auto& e : args)
                std::cerr << e << std::endl;

            auto proc = bp::launch(exe, args, ctx);

            auto log_path = kdir / symbolic_log_name;

            {
                fs::ofstream ofs(log_path);
                if(!ofs.good())
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{log_path.string()});
                }

                bp::pistream& is = proc.get_stdout();
                std::string line;
                while(std::getline(is, line))
                {
                    ofs << line << '\n';
                }
            }

            auto status = proc.wait();

            if(!process::is_exit_status_zero(status))
            {
                BOOST_THROW_EXCEPTION(SymbolicExecException{retrieve_tests(kdir)} << err::process_exit_status{exe});
            }

            if(!util::debug::is_last_line_correct(log_path))
            {
                BOOST_THROW_EXCEPTION(SymbolicExecException{retrieve_tests(kdir)} << err::process{exe});
            }

        }
        , fsm.trace_dir_
        , fsm.dispatch_options_
        , fsm.node_options_});
    }
};

struct KleeFSM_::retrieve_result
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir,
                                              std::shared_ptr<std::vector<TestCase>> tests)
        {
            auto kdir = trace_dir / klee_dir_name;

            *tests = retrieve_tests(kdir);


        }, fsm.trace_dir_, fsm.tests_});
    }
};

struct KleeFSM_::clean
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.tests_->clear();

        if(fs::remove_all(fsm.trace_dir_) == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file{"failed to remove trace directory"});
        }
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+
struct KleeFSM_::is_prev_task_finished
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState& ss, TargetState&) -> bool
    {
        if(ss.async_task_->is_exception_thrown())
        {
            auto e = ss.async_task_->release_exception();

            std::rethrow_exception(e);
        }

        return ss.async_task_->is_finished();
    }
};


// Normally would just: "using QemuFSM = boost::msm::back::state_machine<KleeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class KleeFSM : public boost::msm::back::state_machine<KleeFSM_>
{
};

} // namespace fsm
} // namespace svm
} // namespace node
} // namespace cluster
} // namespace crete
