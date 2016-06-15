#ifndef CRETE_COVERAGE_H
#define CRETE_COVERAGE_H

#include <crete/executor.h>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>

#include <string>

namespace crete
{

void generate_report(const boost::filesystem::path& working);
void generate_report(const boost::filesystem::path& working, const std::string& lcov_args);
void generate_html(const boost::filesystem::path& working);

class CoverageExecutor : public Executor
{
public:
    CoverageExecutor(const boost::filesystem::path& binary,
                     const boost::filesystem::path& test_case_dir,
                     const boost::filesystem::path& configuration);
protected:
    void clean() override;

private:
};

class Coverage
{
public:
    Coverage(int argc, char* argv[]);

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
