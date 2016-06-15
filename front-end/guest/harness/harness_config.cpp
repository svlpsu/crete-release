#include "crete/harness_config.h"

#include <sstream>
#include <fstream>
#include <vector>
#include <iostream> // testing

#include <stdlib.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/functional/hash.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>

namespace pt = boost::property_tree;
namespace fs = boost::filesystem;

namespace crete
{
namespace config
{

HarnessConfiguration::HarnessConfiguration() :
    first_iteration_(false)
{
}

HarnessConfiguration::HarnessConfiguration(const boost::property_tree::ptree& config_tree) :
    first_iteration_(false)
{
    stdin_.size = 0;
    stdin_.concolic = false;

    HarnessConfiguration::load_configuration(config_tree);
}

HarnessConfiguration::~HarnessConfiguration()
{
}

void HarnessConfiguration::load_configuration(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_crete = config_tree.get_child_optional("crete");
    if(!opt_crete)
    {
        throw std::runtime_error("[CRETE] configuration error - missing root node 'crete'");
    }

    const pt::ptree& crete_tree = *opt_crete;

    load_arguments(crete_tree);
    load_files(crete_tree);
    load_stdin(crete_tree);
}

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

        fs::path fp = file.path;

        if(!fs::exists(fp))
        {
            throw std::runtime_error("failed to find file: " + fp.string());
        }

        std::ifstream ifs(fp.string().c_str());

        std::size_t fsize = fs::file_size(fp);

        file.data.resize(fsize);
        ifs.read(reinterpret_cast<char*>(file.data.data()), fsize);
        // TODO: check if bytes read successfully?
        file.size = fsize;
    }
}

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

Arguments HarnessConfiguration::get_arguments() const
{
    return arguments_;
}

Files HarnessConfiguration::get_files() const
{
    return files_;
}

void HarnessConfiguration::write(boost::property_tree::ptree& config) const
{
    pt::ptree& args_node = config.put_child("crete.args", pt::ptree());
    pt::ptree& files_node = config.put_child("crete.files", pt::ptree());

    BOOST_FOREACH(const Argument& arg, arguments_)
    {
        pt::ptree& arg_node = args_node.add_child("arg", pt::ptree());
        arg_node.put("<xmlattr>.index", arg.index);
        arg_node.put("<xmlattr>.size", arg.size);
        arg_node.put("<xmlattr>.value", arg.value);
        arg_node.put("<xmlattr>.concolic", arg.concolic);
    }

    BOOST_FOREACH(const File& file, files_)
    {
        pt::ptree& file_node = files_node.add_child("file", pt::ptree());
        file_node.put("<xmlattr>.path", file.path.string());
        file_node.put("<xmlattr>.virtual", !file.real);
        file_node.put("<xmlattr>.size", file.size);
    }
}

bool HarnessConfiguration::is_first_iteration() const
{
    return first_iteration_;
}

void HarnessConfiguration::is_first_iteration(bool b)
{
    first_iteration_ = b;
}

void HarnessConfiguration::load_arguments(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_args = config_tree.get_child_optional("args");

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

void HarnessConfiguration::load_files(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_files = config_tree.get_child_optional("files");

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

STDStream HarnessConfiguration::get_stdin() const
{
    return stdin_;
}

void HarnessConfiguration::load_stdin(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_stdin = config_tree.get_child_optional("stdin");

    if(!opt_stdin)
    {
        return;
    }

    const pt::ptree& stdin_config = *opt_stdin;

    stdin_.size = stdin_config.get<std::size_t>("<xmlattr>.size");
    stdin_.concolic = stdin_config.get<std::size_t>("<xmlattr>.concolic", true);
}

} // namespace config
} // namespace crete
