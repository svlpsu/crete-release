#ifndef CRETE_HARNESS_CONFIG_H
#define CRETE_HARNESS_CONFIG_H

#include <string>
#include <vector>

#include <crete/asio/client.h>
#include <crete/elf_reader.h>
#include <crete/proc_reader.h>

#include <boost/serialization/split_member.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

namespace crete
{
namespace config
{

struct File
{
    boost::filesystem::path path;
    bool real;
    uint64_t size;
    std::vector<uint8_t> data;

    File() :
        real(true),
        size(0)
    {
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string path = this->path.generic_string();
        ar & BOOST_SERIALIZATION_NVP(path);
        ar & BOOST_SERIALIZATION_NVP(real);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(data);
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;
        std::string path;

        ar & BOOST_SERIALIZATION_NVP(path);
        ar & BOOST_SERIALIZATION_NVP(real);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(data);

        this->path = boost::filesystem::path(path);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<File> Files;

struct Argument
{
    std::size_t index;
    std::size_t size;
    std::string value;
    bool concolic;

    Argument() :
        index(0),
        size(0),
        concolic(false)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(index);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(value);
        ar & BOOST_SERIALIZATION_NVP(concolic);
    }
};

typedef std::vector<Argument> Arguments;

struct ArgMinMax
{
    ArgMinMax() :
        concolic(false),
        min(0),
        max(0)
    {
    }

    bool concolic;
    uint32_t min;
    uint32_t max;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(concolic);
        ar & BOOST_SERIALIZATION_NVP(min);
        ar & BOOST_SERIALIZATION_NVP(max);
    }
};

struct STDStream
{
    std::size_t size;
    bool concolic;

    STDStream() :
        size(0),
        concolic(false)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(concolic);
    }
};

class HarnessConfiguration
{
public:
    HarnessConfiguration();
    HarnessConfiguration(const boost::property_tree::ptree& config_tree);
    ~HarnessConfiguration();

    void load_configuration(const boost::property_tree::ptree& config_tree);

    void load_file_data();
    void clear_file_data();

    Arguments get_arguments() const;
    Files get_files() const;
    STDStream get_stdin() const;

    void write(boost::property_tree::ptree& config) const;

    bool is_first_iteration() const;
    void is_first_iteration(bool b);

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const;
    template <class Archive>
    void load(Archive& ar, const unsigned int version);
    BOOST_SERIALIZATION_SPLIT_MEMBER()

protected:
    void load_files(const boost::property_tree::ptree& config_tree);
    void load_file(const boost::property_tree::ptree& config_tree);
    void load_arguments(const boost::property_tree::ptree& config_tree);
    void load_argument(const boost::property_tree::ptree& config_tree);
    void load_stdin(const boost::property_tree::ptree& config_tree);

private:
    ArgMinMax argminmax_;
    Arguments arguments_;
    Files files_;
    STDStream stdin_;
    bool first_iteration_;
};

template <class Archive>
void HarnessConfiguration::save(Archive& ar, const unsigned int version) const
{
    (void)version;

    ar & BOOST_SERIALIZATION_NVP(argminmax_);
    ar & BOOST_SERIALIZATION_NVP(arguments_);
    ar & BOOST_SERIALIZATION_NVP(files_);
    ar & BOOST_SERIALIZATION_NVP(stdin_);
    ar & BOOST_SERIALIZATION_NVP(first_iteration_);
}

template <class Archive>
void HarnessConfiguration::load(Archive& ar, const unsigned int version)
{
    (void)version;

    ar & BOOST_SERIALIZATION_NVP(argminmax_);
    ar & BOOST_SERIALIZATION_NVP(arguments_);
    ar & BOOST_SERIALIZATION_NVP(files_);
    ar & BOOST_SERIALIZATION_NVP(stdin_);
    ar & BOOST_SERIALIZATION_NVP(first_iteration_);
}

inline
HarnessConfiguration::HarnessConfiguration() :
    first_iteration_(false)
{
}

inline
HarnessConfiguration::HarnessConfiguration(const boost::property_tree::ptree& config_tree) :
    first_iteration_(false)
{
    stdin_.size = 0;
    stdin_.concolic = false;

    HarnessConfiguration::load_configuration(config_tree);
}

inline
HarnessConfiguration::~HarnessConfiguration()
{
}

inline
void HarnessConfiguration::load_configuration(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_crete = config_tree.get_child_optional("crete");
    if(!opt_crete)
    {
        throw std::runtime_error("[CRETE] configuration error - missing root node 'crete'");
    }

    const boost::property_tree::ptree& crete_tree = *opt_crete;

    load_arguments(crete_tree);
    load_files(crete_tree);
    load_stdin(crete_tree);
}

inline
void HarnessConfiguration::load_file_data()
{
    for(Files::iterator it = files_.begin();
        it != files_.end();
        ++it)
    {
        File& file = *it;

        if(!file.real)
        {
            continue;
        }

        boost::filesystem::path fp = file.path;

        if(!boost::filesystem::exists(fp))
        {
            throw std::runtime_error("failed to find file: " + fp.string());
        }

        std::ifstream ifs(fp.string().c_str());

        std::size_t fsize = boost::filesystem::file_size(fp);

        file.data.resize(fsize);
        ifs.read(reinterpret_cast<char*>(file.data.data()), fsize);
        // TODO: check if bytes read successfully?
        file.size = fsize;
    }
}

inline
void HarnessConfiguration::clear_file_data()
{
    for(Files::iterator it = files_.begin();
        it != files_.end();
        ++it)
    {
        File& file = *it;

        file.data.clear();
    }
}

inline
Arguments HarnessConfiguration::get_arguments() const
{
    return arguments_;
}

inline
Files HarnessConfiguration::get_files() const
{
    return files_;
}

inline
void HarnessConfiguration::write(boost::property_tree::ptree& config) const
{
    boost::property_tree::ptree& args_node = config.put_child("crete.args", boost::property_tree::ptree());
    boost::property_tree::ptree& files_node = config.put_child("crete.files", boost::property_tree::ptree());

    BOOST_FOREACH(const Argument& arg, arguments_)
    {
        boost::property_tree::ptree& arg_node = args_node.add_child("arg", boost::property_tree::ptree());
        arg_node.put("<xmlattr>.index", arg.index);
        arg_node.put("<xmlattr>.size", arg.size);
        arg_node.put("<xmlattr>.value", arg.value);
        arg_node.put("<xmlattr>.concolic", arg.concolic);
    }

    BOOST_FOREACH(const File& file, files_)
    {
        boost::property_tree::ptree& file_node = files_node.add_child("file", boost::property_tree::ptree());
        file_node.put("<xmlattr>.path", file.path.string());
        file_node.put("<xmlattr>.virtual", !file.real);
        file_node.put("<xmlattr>.size", file.size);
    }
}

inline
bool HarnessConfiguration::is_first_iteration() const
{
    return first_iteration_;
}

inline
void HarnessConfiguration::is_first_iteration(bool b)
{
    first_iteration_ = b;
}

inline
void HarnessConfiguration::load_arguments(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_args = config_tree.get_child_optional("args");

    if(!opt_args)
    {
        return;
    }

    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  *opt_args)
    {
        load_argument(v.second);
    }
}

inline
void HarnessConfiguration::load_argument(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    Argument arg;

    arg.index = config_tree.get<std::size_t>("<xmlattr>.index");
    arg.size = config_tree.get<std::size_t>("<xmlattr>.size", 0);
    arg.value = config_tree.get<std::string>("<xmlattr>.value", "");
    arg.concolic = config_tree.get<bool>("<xmlattr>.concolic", true);

    if(arg.size == 0)
    {
        if(arg.value.empty())
        {
            throw std::runtime_error("size is 0 and value is empty for arg");
        }
        else
        {
            arg.size = arg.value.size();
        }
    }

    for(Arguments::const_iterator it = arguments_.begin();
        it != arguments_.end();
        ++it)
    {
        if(arg.index == it->index)
        {
            throw std::runtime_error("duplicate argument index");
        }
    }

    arguments_.push_back(arg);
}

inline
void HarnessConfiguration::load_files(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_files = config_tree.get_child_optional("files");

    if(!opt_files)
    {
        return;
    }

    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  *opt_files)
    {
        load_file(v.second);
    }
}

inline
void HarnessConfiguration::load_file(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    File file;

    file.path = config_tree.get<fs::path>("<xmlattr>.path");
    file.real = !config_tree.get<bool>("<xmlattr>.virtual", false);
    if(!file.real)
    {
        file.size = config_tree.get<std::size_t>("<xmlattr>.size");
    }
    else
    {
        file.size = 0;
    }

    if(file.real && !fs::exists(file.path))
    {
        throw std::runtime_error("file not found: " + file.path.string());
    }

    files_.push_back(file);
}

inline
STDStream HarnessConfiguration::get_stdin() const
{
    return stdin_;
}

inline
void HarnessConfiguration::load_stdin(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_stdin = config_tree.get_child_optional("stdin");

    if(!opt_stdin)
    {
        return;
    }

    const boost::property_tree::ptree& stdin_config = *opt_stdin;

    stdin_.size = stdin_config.get<std::size_t>("<xmlattr>.size");
    stdin_.concolic = stdin_config.get<std::size_t>("<xmlattr>.concolic", true);
}

} // namespace config
} // namespace crete

#endif // CRETE_HARNESS_CONFIG_H
