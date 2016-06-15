#ifndef CRETE_COVERAGE_H
#define CRETE_COVERAGE_H

#include <crete/executor.h>
#include <crete/proc_reader.h>

#include "run_config.h"

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>

#include <string>

namespace crete
{

void generate_report(const boost::filesystem::path& working);
void generate_report(const boost::filesystem::path& working, const std::string& lcov_args);
void generate_html(const boost::filesystem::path& working);

class GuestReplayExecutor : public Executor
{
public:
    GuestReplayExecutor(const boost::filesystem::path& binary,
                        const boost::filesystem::path& test_case_dir,
                        const boost::filesystem::path& configuration,
                        bool trace_mode);
    ~GuestReplayExecutor();

protected:
    void clean() override;
    void execute() override;

    void initialize_configuration(const boost::filesystem::path& configuration);
    void initialize_proc_reader();

    void reset_timer();
    void start_timer();
    void stop_timer();
    void send_dump_instr();

    void prime();
    void set_call_depth();
    void load_defaults();
    void process_filters();
    void process_function_entries();
    void process_executable_function_entries(const std::vector<Entry>& entries);
    void process_library_function_entries(const std::vector<Entry>& entries, uint64_t base_addr, std::string path);
    void process_func_filter(ELFReader& reader, ProcReader& pr, const config::Functions& funcs, void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_lib_filter(ProcReader& pr, const std::vector<std::string>& libs, void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_executable_section(ELFReader& reader, const std::vector<std::string>& sections, void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_call_stack_library_exclusions(ELFReader& er, const ProcReader& pr);
    void process_call_stack_library_exclusions(ELFReader& er, const ProcReader& pr, const std::vector<boost::filesystem::path>& libraries);
    void process_library_sections();
    void process_library_section(ELFReader& reader, const std::vector<std::string>& sections, void (*f_custom_instr)(uintptr_t, uintptr_t), uint64_t base_addr);
    boost::filesystem::path deduce_library(const boost::filesystem::path& lib, const ProcReader& pr);
    void validate_final_checks() const;

private:
    ProcReader proc_reader_;
    config::RunConfiguration config_;
    bool trace_mode_;
    bool libc_main_found_;
    bool libc_exit_found_;
};

class GuestReplay
{
public:
    GuestReplay(int argc, char* argv[]);

protected:
    void parse_options(int argc, char* argv[]);
    boost::program_options::options_description make_options();
    void process_options();

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
    boost::filesystem::path exec_;
    boost::filesystem::path tc_dir_;
    boost::filesystem::path config_;
};

}

#endif // CRETE_COVERAGE_H
