#include <crete/cluster/klee.h>
#include <crete/exception.h>
#include <crete/process.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/process.hpp>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace bui = boost::uuids;

namespace crete
{
namespace cluster
{

Klee::Klee(const Trace& trace,
           const boost::filesystem::path& working_dir) :
    trace_(trace),
    working_dir_(working_dir)
{
    std::cout << "Starting Klee instance" << std::endl;
}

Klee::~Klee()
{
    std::cout << "Exiting Klee instance" << std::endl;
}

auto Klee::is_concolic_started() const -> bool
{
    return is_concolic_started_; // Concolic precedes symbolic.
}

auto Klee::is_symbolic_started() const -> bool
{
    return is_symbolic_started_; // Concolic precedes symbolic.
}

auto Klee::is_concolic_finished() const -> bool
{
    return is_concolic_started() && !process::is_running(pid_concolic_); // TODO: possible pid is reused before this is checked? Unlikely, but possible?
}

auto Klee::is_symbolic_finished() const -> bool
{
    return is_concolic_started() && !process::is_running(pid_symbolic_); // TODO: possible pid is reused before this is checked? Unlikely, but possible?
}

auto Klee::test_cases() -> const TestCases&
{
    return test_cases_;
}

auto Klee::retrieve_result() -> void
{
    auto test_pool_dir = working_dir_ / bui::to_string(trace_.uuid_) / "klee-run/ktest_pool";

    fs::directory_iterator end;
    for(fs::directory_iterator iter(test_pool_dir); iter != end; ++iter)
    {
        auto test_path = *iter;

        fs::ifstream tests_file(test_path);

        test_cases_.emplace_back(read_test_case(tests_file));
    }
}

KleeExecutor::KleeExecutor(AtomicGuard<Klee>& klee,
                           const Trace& trace) :
    klee_(klee)
{
}

auto Klee::prepare() -> void
{
    fs::path dir = working_dir_ / bui::to_string(trace_.uuid_);
    fs::path kdir = dir / "klee-run";
    auto copy_files = std::vector<std::string>{"main_function.ll",
                                               "dump_llvm.bc",

                                               "dump_mo_symbolics.txt",
                                               "concrete_inputs.bin",

                                               "dump_tbPrologue_regs.bin",
                                               "dump_sync_memos.bin",
                                               "dump_qemu_interrupt_info.bin" };

    if(!fs::exists(dir))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{dir.string()});
    }

    bp::context ctx;
    ctx.work_directory = dir.string();
    ctx.environment = bp::self::get_environment();

    {
        auto script_path = fs::current_path() / std::string{"command_translate.sh"};
        if(fs::exists(script_path)) // TODO: replace with boost::program_options
        {
            auto exe = std::string{"/bin/sh"};
            auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                                 "-c",
                                                 script_path.c_str()};

            auto proc = bp::launch(exe, args, ctx);
            proc.wait();
        }
        else
        {
            auto exe = bp::find_executable_in_path("crete-llvm-translator-i386"); // TODO: what about x64? Info should be sent by dispatch...
            auto args = std::vector<std::string>{fs::absolute(exe).string()}; // It appears our modified QEMU requires full path in argv[0]...

            auto proc = bp::launch(exe, args, ctx);
            auto status = proc.wait();

            if(status.exit_status() != 0)
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
            }

            fs::rename(dir / "dump_llvm_offline.bc",
                       dir / "dump_llvm.bc");
            fs::remove(dir / "dump_tcg_llvm_offline.bin");
        }
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
        auto exe = bp::find_executable_in_path("llvm-as");
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                             "main_function.ll",
                                             "-o=" + std::string("main_function.bc")};
        //        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
        //                                             (kdir/"main_function.ll").string(),
        //                                             "-o=" + (kdir/"main_function.bc").string()};

        auto proc = bp::launch(exe, args, ctx);
        auto status = proc.wait();

        if(status.exit_status() != 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
        }
    }

    {
        auto exe = bp::find_executable_in_path("llvm-link");
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                             "main_function.bc",
                                             "dump_llvm.bc",
                                             "-o",
                                             "run.bc"};
//        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
//                                             (kdir/"main_function.bc").string(),
//                                             (kdir/"dump_llvm.bc").string(),
//                                             "-o",
//                                             (kdir/"run.bc").string()};

        auto proc = bp::launch(exe, args, ctx);
        auto status = proc.wait();

        if(status.exit_status() != 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::process_exit_status{exe});
        }
    }
}

auto Klee::execute_concolic() -> void
{
    prepare();

    is_concolic_started_ = true;

    auto kdir = working_dir_ / bui::to_string(trace_.uuid_) / "klee-run";

    bp::context ctx;
    ctx.work_directory = kdir.string();
    ctx.environment = bp::self::get_environment();

    auto script_path = fs::current_path() / std::string{"command_klee_concolic.sh"};
    if(fs::exists(script_path)) // TODO: replace with boost::program_options
    {
        auto exe = std::string{"/bin/sh"};
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                             "-c",
                                             script_path.c_str()};

        auto proc = bp::launch(exe, args, ctx);
        pid_concolic_ = proc.get_id();
    }
    else // TODO: redirect output to concolic.log, klee-run.log?
    {
        auto exe = bp::find_executable_in_path("klee");
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                         "-search=dfs",
                         "-randomize-fork=false",
                         "-concolic-mode=true",
                         "run.bc",
                         };

        auto proc = bp::launch(exe, args, ctx);
        pid_concolic_ = proc.get_id();
    }
}

auto Klee::execute_symbolic() -> void
{
    is_symbolic_started_ = true;

    auto kdir = working_dir_ / bui::to_string(trace_.uuid_) / "klee-run";

    bp::context ctx;
    ctx.work_directory = kdir.string();
    ctx.environment = bp::self::get_environment();

    // TESTING
    ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    ctx.stdout_behavior = bp::redirect_stream_to_stdout();

    auto script_path = fs::current_path() / std::string{"command_klee_symbolic.sh"};
    if(fs::exists(script_path)) // TODO: replace with boost::program_options
    {
        auto exe = std::string{"/bin/sh"};
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                             "-c",
                                             script_path.c_str()};

        auto proc = bp::launch(exe, args, ctx);
        pid_symbolic_ = proc.get_id();
    }
    else // TODO: redirect output to concolic.log, klee-run.log?
    {
        auto exe = bp::find_executable_in_path("klee");
        auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                             "-search=dfs",
                                             "-randomize-fork=false",
                                             "run.bc",
                                             };

        auto proc = bp::launch(exe, args, ctx);
        pid_symbolic_ = proc.get_id();
    }
}

} // namespace cluster
} // namespace crete

