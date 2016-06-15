#ifndef CRETE_GUEST_RUN_CONFIG_H
#define CRETE_GUEST_RUN_CONFIG_H

#include <string>
#include <vector>


#include <boost/serialization/split_member.hpp>

#include "crete/harness_config.h"

namespace crete
{
namespace config
{

struct Preload
{
    boost::filesystem::path lib;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string lib = this->lib.generic_string();

        ar & BOOST_SERIALIZATION_NVP(lib);
    }
    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;

        std::string lib;

        ar & BOOST_SERIALIZATION_NVP(lib);

        this->lib = boost::filesystem::path(lib);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<Preload> Preloads;

struct Function
{
    std::string name;
    boost::filesystem::path lib;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string lib = this->lib.generic_string();

        ar & BOOST_SERIALIZATION_NVP(name);
        ar & BOOST_SERIALIZATION_NVP(lib);
    }
    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;

        std::string lib;

        ar & BOOST_SERIALIZATION_NVP(name);
        ar & BOOST_SERIALIZATION_NVP(lib);

        this->lib = boost::filesystem::path(lib);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<Function> Functions;

struct CodeSelectionBounds
{
    int32_t low;
    int32_t high;

    CodeSelectionBounds() :
        low(0),
        high(0)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(low);
        ar & BOOST_SERIALIZATION_NVP(high);
    }
};

struct Exploration
{
    uint32_t call_depth;
    CodeSelectionBounds stack_depth;
    std::string strategy;
    // std::vector<Heuristic> heuristics;

    Exploration() :
        call_depth(0)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(call_depth);
        ar & BOOST_SERIALIZATION_NVP(stack_depth);
        ar & BOOST_SERIALIZATION_NVP(strategy);
    }

};

class RunConfiguration : public HarnessConfiguration
{
public:
    typedef boost::filesystem::path Executable;
    typedef std::string Library; // TODO: is there a good reason the type isn't fs::path? Serialization, but I've overcome that problem.
    typedef std::vector<Library> Libraries;
    typedef std::vector<std::string> SectionExclusions;

public:
    RunConfiguration();
    RunConfiguration(const boost::property_tree::ptree& config_tree);

    Preloads get_preloads() const;
    Executable get_executable() const;
    Functions get_include_functions() const;
    Functions get_exclude_functions() const;
    Libraries get_libraries() const;
    Exploration get_exploration() const;
    SectionExclusions get_section_exclusions() const;

    void set_executable(const Executable& exe);

    void load_preloads(const boost::property_tree::ptree& config_tree);
    void load_preload(const boost::property_tree::ptree& config_tree);
    void load_executable(const boost::property_tree::ptree& config_tree);
    void load_functions(const boost::property_tree::ptree& config_tree);
    void load_function(const boost::property_tree::ptree& config_tree, Functions& functions);
    void load_libraries(const boost::property_tree::ptree& config_tree);
    void load_exploration(const boost::property_tree::ptree& config_tree);
    void load_call_depth(const boost::property_tree::ptree& config_tree);
    void load_stack_depth(const boost::property_tree::ptree& config_tree);
    void load_strategy(const boost::property_tree::ptree& config_tree);
    void load_sections(const boost::property_tree::ptree& config_tree);

    void write(boost::property_tree::ptree& config) const;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const;
    template <class Archive>
    void load(Archive& ar, const unsigned int version);
    BOOST_SERIALIZATION_SPLIT_MEMBER()

protected:
    void load_configuration(const boost::property_tree::ptree& config_tree);
    void validate(Exploration& exploration);

private:
    Preloads preloads_;
    Executable executable_;
    Functions include_functions_;
    Functions exclude_functions_;
    Libraries libraries_;
    Exploration exploration_;

    // Internal.
    SectionExclusions section_exclusions_; // i.e., call-stack exclusion.
};

template <class Archive>
void RunConfiguration::save(Archive& ar, const unsigned int version) const
{
    HarnessConfiguration::save(ar, version);

    (void)version;

    std::string executable = executable_.generic_string();

    ar & BOOST_SERIALIZATION_NVP(preloads_);
    ar & BOOST_SERIALIZATION_NVP(executable);
    ar & BOOST_SERIALIZATION_NVP(include_functions_);
    ar & BOOST_SERIALIZATION_NVP(exclude_functions_);
    ar & BOOST_SERIALIZATION_NVP(libraries_);
    ar & BOOST_SERIALIZATION_NVP(exploration_);
    ar & BOOST_SERIALIZATION_NVP(section_exclusions_);
}

template <class Archive>
void RunConfiguration::load(Archive& ar, const unsigned int version)
{
    HarnessConfiguration::load(ar, version);

    (void)version;

    std::string executable;

    ar & BOOST_SERIALIZATION_NVP(preloads_);
    ar & BOOST_SERIALIZATION_NVP(executable);
    ar & BOOST_SERIALIZATION_NVP(include_functions_);
    ar & BOOST_SERIALIZATION_NVP(exclude_functions_);
    ar & BOOST_SERIALIZATION_NVP(libraries_);
    ar & BOOST_SERIALIZATION_NVP(exploration_);
    ar & BOOST_SERIALIZATION_NVP(section_exclusions_);

    executable_ = boost::filesystem::path(executable);
}

} // namespace config
} // namespace crete

#endif // CRETE_GUEST_RUN_CONFIG_H
