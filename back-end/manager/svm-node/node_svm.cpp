#include "node_svm.h"

#include <iomanip>
#include <iostream>
#include <boost/thread.hpp>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/move/make_unique.hpp>

#include <crete/exception.h>
#include <crete/cluster/node_driver.h>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

namespace crete
{
namespace cluster
{

SVMNodeUI::SVMNodeUI(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

SVMNodeUI::~SVMNodeUI()
{
    if(node_driver_thread_.joinable())
    {
        node_driver_thread_.join();
    }
}

auto SVMNodeUI::run() -> void
{
    std::cout << "[CRETE] Connecting to master '"
              << node_options_.master.ip
              << "' on port '"
              << node_options_.master.port
              << "' ..."
              << std::endl;

    node_ = boost::movelib::make_unique<AtomicGuard<SVMNode>>(node_options_);

    node_driver_thread_ = boost::thread{NodeDriver<SVMNode>{node_options_.master.ip,
                                                            node_options_.master.port,
                                                            *node_}};
}

auto SVMNodeUI::parse_options(int argc, char* argv[]) -> void
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

auto SVMNodeUI::make_options() -> po::options_description
{
    po::options_description desc("Options");

    desc.add_options()
            ("help,h", "displays help message")
            ("config,c", po::value<fs::path>(), "configuration file")
            ("instances,n", po::value<size_t>(), "[required] number of svm instances")
            ("ip,i", po::value<std::string>(), "master IP address")
            ("port,p", po::value<Port>(), "master port")
        ;

    return desc;
}

auto SVMNodeUI::process_options() -> void
{
    using namespace std;

//    if(var_map_.size() == 0)
//    {
//        cout << "Missing arguments" << endl;
//        cout << "Use '--help' for more details" << endl;

//        BOOST_THROW_EXCEPTION(Exception{} << err::arg_missing{"required arguments not provided"});
//    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;

        BOOST_THROW_EXCEPTION(Exception{});
    }
    if(var_map_.count("config")) // Must be processed first.
    {
        auto config_path = var_map_["config"].as<fs::path>();

        if(!fs::exists(config_path))
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{config_path.string()});
        }

        fs::ifstream ifs(config_path);

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{config_path.string()});
        }

        pt::ptree config;

        pt::read_xml(ifs,
                     config);

        node_options_ = node::option::SVMNode{config};
    }
    if(var_map_.count("instances"))
    {
        node_options_.svm.count = var_map_["instances"].as<size_t>();

        if(node_options_.svm.count == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{"instances"});
        }
    }
    if(var_map_.count("ip"))
    {
        node_options_.master.ip = var_map_["ip"].as<std::string>();
    }
    if(var_map_.count("port"))
    {
        node_options_.master.port = var_map_["port"].as<Port>();
    }
}

} // namespace cluster
} // namespace crete

int main(int argc, char* argv[])
{
    using namespace std;
    using namespace crete;

    try
    {
        cluster::SVMNodeUI node(argc, argv);

        node.run();
    }
    catch(Exception& e)
    {
        cerr << "[CRETE] Exception: " << boost::diagnostic_information(e) << endl;
        return -1;
    }
    catch(std::exception& e)
    {
        cerr << "[std] Exception: " <<  boost::diagnostic_information(e) << endl;
        return -1;
    }
    catch(...)
    {
        cerr << "[...] Exception: " <<  boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
