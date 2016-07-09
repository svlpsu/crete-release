#ifndef CRETE_GUEST_RUN_H
#define CRETE_GUEST_RUN_H

#include <vector>
#include <memory>
#include <string>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/path.hpp>

namespace crete
{

const std::string log_file_name = "run.log";
const std::string proc_maps_file_name = "proc-maps.log";
const std::string harness_config_file_name = "harness.config.serialized";

class RunnerFSM;

class Runner
{
public:
    Runner(int argc, char* argv[]);

    boost::program_options::options_description make_options();
    void parse_options(int argc, char* argv[]);
    void process_options();

    void run();
    void start_FSM();
    void stop(); // TODO: useful?

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
    boost::shared_ptr<RunnerFSM> fsm_;
    std::string ip_;
    boost::filesystem::path target_config_;
    bool stopped_;
};

} // namespace crete

#endif // CRETE_GUEST_RUN_H
