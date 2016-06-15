#include "coverage.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include <string>

using namespace std;
using namespace boost;
namespace fs = filesystem;
namespace po = program_options;

namespace crete
{

CoverageExecutor::CoverageExecutor(const filesystem::path& binary,
                                   const filesystem::path& test_case_dir,
                                   const boost::filesystem::path& configuration) :
    Executor(binary,
             test_case_dir,
             configuration)
{
}

void generate_report(const fs::path& working)
{
    string lcov = "lcov -t 'test' -o " +
            working.generic_string() +
            "/bin_coverage.info -c -d " +
            working.generic_string() + " &> /dev/null";
    std::system(lcov.c_str());
}

void generate_report(const fs::path& working,
                     const string& lcov_args)
{
    string lcov = "lcov -t 'test' -o " +
            working.generic_string() +
            "/bin_coverage.info -c " +
            lcov_args + " > /dev/null";
    std::system(lcov.c_str());
}

void CoverageExecutor::clean()
{
    Executor::clean();
}

void generate_html(const filesystem::path& working)
{
    string genhtml = "genhtml -o " +
            working.generic_string() +
            "/html " +
            working.generic_string() +
            "/bin_coverage.info | grep \"coverage rate\" -A3";
    std::system(genhtml.c_str());
}

Coverage::Coverage(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

void Coverage::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

po::options_description Coverage::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("config,c", po::value<fs::path>(), "configuration file (found in guest-data/)")
        ("exec,e", po::value<fs::path>(), "executable to test")
        ("gen-html,g", "generate an html report")
        ("lcov-args,l", po::value<std::string>(), "additional arguments to lcov")
        ("reset,r", po::value<fs::path>(), "[unimplemented] clears all previous execution data")
        ("tc-dir,t", po::value<fs::path>(), "test case directory")
        ;

    return desc;
}

void Coverage::process_options()
{
    std::string lcov_args;
    bool gen_html = false;

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
    if(var_map_.count("lcov-args"))
    {
        lcov_args = var_map_["lcov-args"].as<std::string>();
    }
    if(var_map_.count("gen-html"))
    {
        gen_html = true;
    }

    if(!exec_.empty())
    {
        CoverageExecutor executor(exec_,
                                  tc_dir_,
                                  config_);

        executor.execute_all();
    }

    auto working_dir = fs::current_path();

    if(lcov_args.empty())
    {
        generate_report(working_dir);
    }
    else
    {
        generate_report(working_dir, lcov_args);
    }

    if(gen_html)
    {
        generate_html(working_dir);
    }

}

} // namespace crete

int main(int argc, char* argv[])
{
    try
    {
        crete::Coverage{argc, argv};
    }
    catch(std::exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
        return -1;
    }

    return 0;
}
