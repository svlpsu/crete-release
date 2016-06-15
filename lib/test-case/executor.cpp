#include <crete/executor.h>
#include <crete/test_case.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/date_time.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp> // Needed for text_iarchive (for some reason).
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/process.hpp>

#include <string>
#include <sstream>
#include <algorithm>

using namespace std;
namespace fs = boost::filesystem;
namespace bp = boost::process;

const char* const crete_test_case_args_path = "test_case_args.bin";
const char* const crete_process_output_path = "crete.output.txt";

namespace crete
{

Executor::Executor(const fs::path& binary,
                   const fs::path& test_case_dir,
                   const boost::filesystem::path& configuration) :
    bin_(fs::absolute(binary)),
    test_case_dir_(fs::absolute(test_case_dir)),
    working_dir_(bin_.parent_path()),
    test_case_count_(0),
    iteration_(0),
    iteration_print_threshold_(0)
{
    load_configuration(configuration);

    fs::remove(working_dir_ / "crete.output.txt");
    count_test_cases();
}

Executor::~Executor()
{
    clean();
}

void Executor::execute_all()
{
    for(fs::directory_iterator iter(test_case_dir_), dend;
        iter != dend;
        ++iter)
    {
        current_tc_ = iter->path();

        if(fs::is_regular_file(current_tc_))
        {
            clean();
            prepare();
            execute();

            print_status(cout);
            ++iteration_;
        }
    }
}

void Executor::clean()
{
    config::Files files = harness_config_.get_files();
    for(config::Files::const_iterator it = files.begin();
        it != files.end();
        ++it)
    {
        fs::remove(working_dir_ / it->path);
    }
}

void Executor::prepare()
{
    fs::ifstream ifs(current_tc_);
    TestCase tc = read_test_case(ifs);

    const TestCaseElements tces = filter(tc.get_elements());

    ElemTypeQueue queue = element_type_sequence();
    config::Arguments args = harness_config_.get_arguments();

    std::deque<config::Argument*> concolic_args;
    for(config::Arguments::iterator it = args.begin();
        it != args.end();
        ++it)
    {
        if(it->concolic)
            concolic_args.push_front(&(*it));
    }

    for(TestCaseElements::const_iterator it = tces.begin();
        it != tces.end();
        ++it)
    {
        ElemType et = queue.back();
        queue.pop_back();

        switch(et)
        {
        case elem_arg:
        {
            config::Argument* arg = concolic_args.back();
            concolic_args.pop_back();

            arg->value = std::string(it->data.begin(), it->data.end());
            if(arg->size != arg->value.size())
            {
                throw std::runtime_error("configuration and test arg size don't match");
            }
            break;
        }
        case elem_file:
        {
            write(*it);
            break;
        }
        case elem_stdin:
        {
            const TestCaseElement& stdin_data = *it;

            current_stdin_data_ = stdin_data.data;

            break;
        }
        }
    }

    current_args_ = args;

    write_arguments_file(current_args_);
}

void Executor::execute()
{
    std::vector<std::string> args = boost::assign::list_of(bin_.filename().generic_string());

    execute(binary().string(), args);
}

void Executor::execute(const string& exe, const std::vector<string>& args)
{
    bp::context ctx;
    ctx.work_directory = working_directory().generic_string();
    if(current_stdin_data_.size() > 0)
    {
        ctx.stdin_behavior = bp::capture_stream();
    }
    ctx.stdout_behavior = bp::capture_stream();
    ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    ctx.environment = bp::self::get_environment();
    ctx.environment.erase("LD_PRELOAD");
    ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_host_preload.so"));

    bp::child proc = bp::launch(exe, args, ctx);

    if(current_stdin_data_.size() > 0)
    {
        bp::postream& os = proc.get_stdin();

        os.write((char*)current_stdin_data_.data(), current_stdin_data_.size());
        os.close();
    }

    bp::pistream& is = proc.get_stdout();

    fs::ofstream ofs(working_directory() / crete_process_output_path,
                     fs::fstream::app | fs::fstream::out);

    std::string line;
    while(getline(is, line))
    {
        ofs << line << std::endl;
    }
}

void Executor::print_status(ostream& os)
{
    std::size_t threshold = iteration_print_threshold_;

    if(threshold == 0)
        threshold = 1;

    if(iteration_ % iteration_print_threshold_ == 0)
    {
        std::streamsize prev_width = os.width();
        std::streamsize prev_precision = os.precision();

        print_status_header(os);
        os << std::endl;
        print_status_details(os);
        os << std::endl;

        os.width(prev_width);
        os.precision(prev_precision);
    }
}

void Executor::print_status_header(ostream& os)
{
    system("clear");
    os   << setw(14) << "processed"
         << "|"
         << setw(14) << "elapsed time"
         << "|";
}

void Executor::print_status_details(ostream& os)
{
    static boost::posix_time::ptime start_time = boost::posix_time::second_clock::local_time();

    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();

    double percentage = static_cast<float>(iteration_ + 1) / test_case_count_;
    percentage *= 100;

    os << setprecision(3);

    os   << setw(13) << percentage << "%"
         << "|"
         << setw(14) << boost::posix_time::to_simple_string(now - start_time)
         << "|";
}

void Executor::count_test_cases()
{
    for(fs::directory_iterator iter(test_case_dir_), dend;
        iter != dend;
        ++iter)
    {
        if(fs::is_regular_file(*iter))
        {
            ++test_case_count_;
        }
    }

    iteration_print_threshold_ = static_cast<std::size_t>(0.01f * test_case_count_);

    if(iteration_print_threshold_ == 0)
    {
        iteration_print_threshold_ = 1;
    }
}

void Executor::load_configuration(const fs::path& path)
{
    fs::ifstream ifs(path);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + path.string());
    }

    try
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> harness_config_;
    }
    catch(...)
    {
        namespace pt = boost::property_tree;
        pt::ptree config_tree;

        pt::read_xml(path.string(), config_tree);

        config::HarnessConfiguration hconfig(config_tree);

        harness_config_ = hconfig;
    }

    load_element_type_sequence(harness_config_);
}

void Executor::load_element_type_sequence(const config::HarnessConfiguration& hconfig)
{
    const config::Arguments args = hconfig.get_arguments();
    std::size_t arg_size = args.size();
    std::size_t file_size = hconfig.get_files().size();

    for(size_t i = 0; i < arg_size; ++i)
    {
        if(args[i].concolic)
            elem_type_seq_.push_front(elem_arg);
    }
    for(size_t i = 0; i < file_size; ++i)
    {
        elem_type_seq_.push_front(elem_file);
    }
    if(hconfig.get_stdin().concolic)
    {
        elem_type_seq_.push_front(elem_stdin);
    }
}

const Executor::ElemTypeQueue&Executor::element_type_sequence() const
{
    return elem_type_seq_;
}

void Executor::write(const TestCaseElement& tce) const
{
    fs::path fpath = working_dir_ / std::string(tce.name.begin(), tce.name.end());

    std::string filename = fpath.filename().string();
//    assert(boost::ends_with(filename, "-posix")); // TODO: temporary until support for non-posix files is added to Executor.
    fpath = fpath.parent_path() / boost::replace_last_copy(filename, "-posix", "");

    if(fs::exists(fpath))
    {
        throw std::runtime_error("existing file matches test element: " + fpath.string());
    }

    fs::ofstream ofs(fpath);

    if(!ofs.good())
    {
        throw std::runtime_error("failed to create file: " + fpath.string());
    }

    std::copy(tce.data.begin(),
              tce.data.end(),
              std::ostream_iterator<uint8_t>(ofs));
}

/**
 * @brief Executor::filter  - Since there are two elements to represent the same concolic file,
 * 		one for libc file routines and one for posix file routines, we need to eliminate one
 * 		or the other depending which was actually used.
 *
 * 		For now, this defaults to POSIX only.
 * @param elems
 * @return
 */
TestCaseElements Executor::filter(const TestCaseElements& elems) const
{
    TestCaseElements new_elems;

    ElemTypeQueue queue = element_type_sequence();

    for(TestCaseElements::const_iterator it = elems.begin();
        it != elems.end();
        ++it)
    {
        ElemType et = queue.back();
        queue.pop_back();

        switch(et)
        {
        case elem_arg:
        {
            new_elems.push_back(*it);
            break;
        }
        case elem_file:
        {
            const TestCaseElement& libc = *it;

            ++it; // Move to adjacent -posix file.

            if(it == elems.end())
            {
                throw std::runtime_error("adjacent '-posix' concolic element not found: " + current_tc_.string());
            }

            const TestCaseElement& posix = *it;

            if(!boost::ends_with(std::string(posix.name.begin(), posix.name.end()),
                                 "-posix"))
            {
                throw std::runtime_error("adjacent '-posix' concolic element not found: " + current_tc_.string());
            }

            assert(libc.data.size() == posix.data.size());

            new_elems.push_back(select(libc, posix));

            break;
        }
        case elem_stdin:
        {
            const TestCaseElement& libc = *it;

            ++it; // Move to adjacent -posix file.

            if(it == elems.end())
            {
                throw std::runtime_error("adjacent '-posix' concolic element not found: " + current_tc_.string());
            }

            const TestCaseElement& posix = *it;

            if(!boost::ends_with(std::string(posix.name.begin(), posix.name.end()),
                                 "-posix"))
            {
                throw std::runtime_error("adjacent '-posix' concolic element not found " + current_tc_.string());
            }

            assert(libc.data.size() == posix.data.size());

            new_elems.push_back(select(libc, posix));

            break;
        }
        }
    }

    return new_elems;
}

bool sort_by_name_pred(const config::Argument& rhs, const config::Argument& lhs)
{
    return rhs.index < lhs.index;
}

config::Arguments Executor::sort_by_index(const config::Arguments& args)
{
    config::Arguments new_args = args;

    std::sort(new_args.begin(),
              new_args.end(),
              sort_by_name_pred);

    return new_args;
}

string Executor::escape(const string& s)
{
    stringstream ss;

    for(string::const_iterator it = s.begin();
        it != s.end();
        ++it)
    {
        char c = *it;

        if(!std::isalnum(c))
        {
            ss << "\\";
        }

        ss << c;
    }

    return ss.str();
}

void Executor::write_arguments_file(const config::Arguments& args)
{
    const config::Arguments sargs = sort_by_index(args);

    fs::ofstream ofs(crete_test_case_args_path);

    boost::archive::text_oarchive oa(ofs);
    oa << sargs;
}

TestCaseElement Executor::select(const TestCaseElement& lhs, const TestCaseElement& rhs) const
{
    assert(lhs.data.size() == rhs.data.size());

    std::vector<uint8_t> zero_v(lhs.data.size(), 0);

    bool lhs_z = (lhs.data == zero_v);

    if(!lhs_z)
    {
        return lhs;
    }
    else
    {
        return rhs;
    }
}

} // namespace crete
