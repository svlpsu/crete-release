#ifndef CRETE_HARNESS_CONFIG_H
#define CRETE_HARNESS_CONFIG_H

#include <string>
#include <vector>

#include <crete/asio/client.h>
#include <crete/elf_reader.h>
#include <crete/proc_reader.h>

#include <boost/serialization/split_member.hpp>
#include <boost/property_tree/ptree.hpp>

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
    virtual ~HarnessConfiguration();

    virtual void load_configuration(const boost::property_tree::ptree& config_tree);

    void load_file_data();
    void clear_file_data();

    Arguments get_arguments() const;
    Files get_files() const;
    STDStream get_stdin() const;

    virtual void write(boost::property_tree::ptree& config) const;

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

} // namespace config
} // namespace crete

#endif // CRETE_HARNESS_CONFIG_H
