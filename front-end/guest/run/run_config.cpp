#include <crete/run_config.h>

#include <sstream>
#include <fstream>
#include <vector>

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

RunConfiguration::RunConfiguration()
{
}

RunConfiguration::RunConfiguration(const boost::property_tree::ptree& config_tree)
{
    RunConfiguration::load_configuration(config_tree);
}

Preloads RunConfiguration::get_preloads() const
{
    return preloads_;
}

void RunConfiguration::load_configuration(const boost::property_tree::ptree& config_tree)
{
    HarnessConfiguration::load_configuration(config_tree);

    boost::optional<const pt::ptree&> opt_crete = config_tree.get_child_optional("crete");
    if(!opt_crete)
    {
        throw std::runtime_error("[CRETE] configuration error - missing root node 'crete'");
    }

    const pt::ptree& crete_tree = *opt_crete;

    load_preloads(crete_tree);
    load_executable(crete_tree);
    load_functions(crete_tree);
    load_libraries(crete_tree);
    load_exploration(crete_tree);
    load_sections(crete_tree);
}

void RunConfiguration::validate(Exploration& exploration)
{
    const char* const strategies[] = {
        "bfs",
        "exp-1",
        "exp-2",
        "random",
        "weighted"
    };
    std::size_t strat_count = sizeof(strategies) / sizeof(char**);

    bool found = false;
    for(std::size_t i = 0; i < strat_count; ++i)
    {
        if(exploration.strategy == strategies[i])
        {
            found = true;
        }
    }
    if(!found)
    {
        std::string strats;
        for(std::size_t i = 0; i < strat_count; ++i)
        {
            strats += strategies[i];
            strats += "\n";
        }

        throw std::runtime_error("invalid exploration strategy: "
                                 + exploration.strategy
                                 + "\nPlease select one of: "
                                 + strats);
    }
}

RunConfiguration::Executable RunConfiguration::get_executable() const
{
    return executable_;
}

Functions RunConfiguration::get_include_functions() const
{
    return include_functions_;
}

Functions RunConfiguration::get_exclude_functions() const
{
    return exclude_functions_;
}

RunConfiguration::Libraries RunConfiguration::get_libraries() const
{
    return libraries_;
}

Exploration RunConfiguration::get_exploration() const
{
    return exploration_;
}

RunConfiguration::SectionExclusions RunConfiguration::get_section_exclusions() const
{
    return section_exclusions_;
}

void RunConfiguration::load_preloads(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_preloads = config_tree.get_child_optional("preloads");

    if(!opt_preloads)
    {
        return;
    }

    const pt::ptree& preloads = *opt_preloads;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  preloads)
    {
        load_preload(v.second);
    }
}

void RunConfiguration::load_preload(const boost::property_tree::ptree& config_tree)
{
    Preload preload;

    preload.lib = config_tree.get<std::string>("<xmlattr>.path");

    preloads_.push_back(preload);
}

void RunConfiguration::load_executable(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    executable_ = config_tree.get<fs::path>("exec");

    if(!fs::exists(executable_))
        throw std::runtime_error("failed to find executable: " + executable_.generic_string());
}

void RunConfiguration::load_functions(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_funcs = config_tree.get_child_optional("funcs");

    if(!opt_funcs)
    {
        return;
    }

    const pt::ptree& funcs = *opt_funcs;

    if(funcs.get_child_optional("include"))
    {
        BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                      funcs.get_child("include") )
        {
            load_function(v.second, include_functions_);
        }
    }
    if(funcs.get_child_optional("exclude"))
    {
        BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                      funcs.get_child("exclude") )
        {
            load_function(v.second, exclude_functions_);
        }
    }
}

void RunConfiguration::load_function(const boost::property_tree::ptree& config_tree,
                                     Functions& functions)
{
    Function func;
    func.name = config_tree.get<std::string>("<xmlattr>.name");

    boost::optional<fs::path> opt_lib = config_tree.get_optional<fs::path>("<xmlattr>.lib");
    if(opt_lib)
    {
        func.lib = *opt_lib;
    }

    functions.push_back(func);
}

void RunConfiguration::load_libraries(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_libs = config_tree.get_child_optional("libs");

    if(!opt_libs)
    {
        return;
    }

    const pt::ptree& libs = *opt_libs;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  libs )
    {
        libraries_.push_back(v.second.get<std::string>("<xmlattr>.path"));
    }
}

void RunConfiguration::load_exploration(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_exploration = config_tree.get_child_optional("exploration");
    if(!opt_exploration)
    {
        return;
    }

    const pt::ptree& exploration = *opt_exploration;

    load_call_depth(exploration);
    load_stack_depth(exploration);
    load_strategy(exploration);
//    load_hueristics(exploration);

    validate(exploration_);
}

void RunConfiguration::load_call_depth(const boost::property_tree::ptree& config_tree)
{
    exploration_.call_depth = config_tree.get<std::size_t>("call-depth", 0);
}

void RunConfiguration::load_stack_depth(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_stack_depth = config_tree.get_child_optional("stack-depth");

    if(opt_stack_depth)
    {
        const pt::ptree& depth = *opt_stack_depth;

        exploration_.stack_depth.low = depth.get<int32_t>("<xmlattr>.low");
        exploration_.stack_depth.high = depth.get<int32_t>("<xmlattr>.high");
    }
}

void RunConfiguration::load_strategy(const boost::property_tree::ptree& config_tree)
{
    exploration_.strategy = config_tree.get<std::string>("strategy");
}

void RunConfiguration::load_sections(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const pt::ptree&> opt_sections = config_tree.get_child_optional("sections.exclusions");

    if(!opt_sections)
    {
        return;
    }

    const pt::ptree& sections = *opt_sections;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  sections)
    {
        section_exclusions_.push_back(v.second.get_value<std::string>());
    }
}

void RunConfiguration::write(boost::property_tree::ptree& config) const
{
    HarnessConfiguration::write(config);

    config.put("crete.exec", executable_.string());

    pt::ptree& preloads = config.put_child("crete.preloads", pt::ptree());
    pt::ptree& funcs = config.put_child("crete.funcs", pt::ptree());
    pt::ptree& ifuncs = funcs.put_child("include", pt::ptree());
    pt::ptree& ofuncs = funcs.put_child("exclude", pt::ptree());
    pt::ptree& libs = config.put_child("crete.libs", pt::ptree());
    pt::ptree& exploration = config.put_child("crete.exploration", pt::ptree());

    BOOST_FOREACH(const Preload& preload, preloads_)
    {
        pt::ptree& preload_node = preloads.add_child("preload", pt::ptree());
        preload_node.put("<xmlattr>.path", preload.lib.string());
    }

    BOOST_FOREACH(const Function& func, include_functions_)
    {
        pt::ptree& func_node = ifuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Function& func, exclude_functions_)
    {
        pt::ptree& func_node = ofuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Library& lib, libraries_)
    {
        pt::ptree& lib_node = libs.add_child("lib", pt::ptree());
        lib_node.put("<xmlattr>.path", lib);
    }

    {
        exploration.put("call-depth", exploration_.call_depth);
        exploration.put("strategy", exploration_.strategy);
    }
}

void RunConfiguration::set_executable(const RunConfiguration::Executable& exe)
{
    executable_ = exe;
}

} // namespace config
} // namespace crete
