#ifndef CRETE_ASIO_COMMON_H
#define CRETE_ASIO_COMMON_H

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_serialization.hpp>

#include <boost/filesystem.hpp>

#include <boost/asio.hpp>

#include <map>
#include <string>

namespace crete
{

const size_t asio_max_msg_size = 32;

typedef unsigned short Port;
typedef std::string IPAddress;

namespace packet_type // Not an enum b/c pre-C++11 enums size types are impl-defined.
{
const uint32_t properties = 0;
const uint32_t update_query = 1;
const uint32_t elf_entries = 2;
const uint32_t guest_configuration = 3;
const uint32_t proc_maps = 4;
const uint32_t cluster_request_vm_node = 5;
const uint32_t cluster_request_svm_node = 6;
const uint32_t cluster_port = 7;
const uint32_t cluster_status_request = 8;
const uint32_t cluster_status = 9;
const uint32_t cluster_shutdown = 10;
const uint32_t cluster_trace = 11;
const uint32_t cluster_port_request = 12;
const uint32_t cluster_next_test = 13;
const uint32_t cluster_test_case = 14;
const uint32_t cluster_trace_request = 15;
const uint32_t cluster_test_case_request = 16;
const uint32_t cluster_request_config = 17;
const uint32_t cluster_config = 18;
const uint32_t cluster_image_info_request = 19;
const uint32_t cluster_image_info = 20;
const uint32_t cluster_image = 21;
const uint32_t cluster_commence = 22;
const uint32_t cluster_reset = 23;
const uint32_t cluster_next_target = 24;
const uint32_t cluster_error_log_request = 25;
const uint32_t cluster_error_log = 26;
}

struct PacketInfo
{
    uint64_t id; // Use: implementation defined. Meant as supplementary to 'type'.
    uint32_t size; // Size of the associated packet.
    uint32_t type; // Type of the associated packet.
};

template <typename Connection, typename T>
void write_serialized_text(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::text_oarchive oa(os);
    oa << t;

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void write_serialized_binary(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::binary_oarchive oa(os);
    oa << t;

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection>
void write_serialized_text(
        Connection& connection,
        PacketInfo& pktinfo,
        const boost::property_tree::ptree& t,
        size_t version)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::text_oarchive oa(os);
    boost::property_tree::save(oa, t, version);

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void write_serialized_text_xml(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(t);

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void read_serialized_text(Connection& connection, T& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename Connection, typename T>
void read_serialized_text(Connection& connection,
                          T& t,
                          uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    if(pkinfo.type != pk_type)
        throw std::runtime_error("packet type mismatch");

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename T>
void read_serialized_text(boost::asio::streambuf& sbuf,
                          T& t)
{
    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename T>
void read_serialized_binary(boost::asio::streambuf& sbuf,
                          T& t)
{
    std::istream is(&sbuf);
    boost::archive::binary_iarchive ia(is);

    ia >> t;
}

template <typename Connection, typename T>
void read_serialized_binary(Connection& connection,
                            T& t,
                            uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    if(pkinfo.type != pk_type)
        throw std::runtime_error("packet type mismatch");

    std::istream is(&sbuf);
    boost::archive::binary_iarchive ia(is);

    ia >> t;
}


template <typename Connection, typename T>
void read_serialized_text_xml(Connection& connection, T& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

#if 0 // Debugging: Write contents to file instead.
    std::istream ist(&sbuf);
    std::ofstream ofs("config.ser");
    std::copy(std::istreambuf_iterator<char>(ist),
              std::istreambuf_iterator<char>(),
              std::ostream_iterator<char>(ofs));
#endif

    std::istream is(&sbuf);
    boost::archive::xml_iarchive ia(is);

    ia >> BOOST_SERIALIZATION_NVP(t);
}


template <typename Connection, typename T>
void read_serialized_text_xml(Connection& connection,
                              T& t,
                              uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    if(pkinfo.type != pk_type)
        throw std::runtime_error("packet type mismatch");

    std::istream is(&sbuf);
    boost::archive::xml_iarchive ia(is);

    ia >> BOOST_SERIALIZATION_NVP(t);
}

template <typename Connection>
void read_serialized_text(
        Connection& connection,
        boost::property_tree::ptree& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    boost::property_tree::load(ia, t, 0);
}

typedef std::map<std::string, std::time_t> QueryFiles;

class UpdateQuery
{
public:
    UpdateQuery() {}
    UpdateQuery(const std::string& dir, const QueryFiles& qfs) : dir_(dir), qfs_(qfs) {}

    const std::string& directory() const { return dir_; }
    const QueryFiles& query_files() const { return qfs_; }

private:
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & dir_;
        ar & qfs_;
    }

    std::string dir_;
    QueryFiles qfs_;
};

class FileInfo
{
public:
    FileInfo(std::string name, uintmax_t size, boost::filesystem::file_type type, boost::filesystem::perms perms) :
        name_(name), size_(size), type_(type), perms_(perms) {}

    std::string path() const { return name_; }
    uintmax_t size() const { return size_; }
    boost::filesystem::file_type type() const { return type_; }
    boost::filesystem::perms perms() const { return perms_; }

private:
    std::string name_;
    uintmax_t size_;
    boost::filesystem::file_type type_;
    boost::filesystem::perms perms_;

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & name_;
        ar & size_;
        ar & type_;
        ar & perms_;
    }
};

class FileTransferList
{
public:
    void push_back(const FileInfo& info) { files_.push_back(info); }

    const std::vector<FileInfo> files() { return files_; }

private:
    std::vector<FileInfo> files_;

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & files_;
    }
};

} // namespace crete

#endif // CRETE_ASIO_COMMON_H
