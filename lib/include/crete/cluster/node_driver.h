#ifndef CRETE_CLUSTER_NODE_DRIVER_H
#define CRETE_CLUSTER_NODE_DRIVER_H

#include <crete/atomic_guard.h>
#include <crete/cluster/common.h>
#include <crete/asio/common.h>
#include <crete/asio/client.h>
#include <crete/exception.h>
#include <crete/test_case.h>
#include <crete/exception_propagator.h>
#include <crete/async_task.h>

#include <boost/thread.hpp>

namespace crete
{
namespace cluster
{

template <typename Node>
class NodeDriver : public ExceptionPropagator
{
public:
    NodeDriver(const IPAddress& master_ipa,
               const Port& master_port,
               AtomicGuard<Node>& node);

    auto operator()() -> void; // Invoked by thread. Calls run().

    auto run_node() -> void;
    auto run_listener() -> void;
    auto retrieve_own_port() const -> Port;
    auto display_status() -> void;

private:
    AtomicGuard<Node>& node_;
    ID node_id_;
    IPAddress master_ip_address_;
    Port master_port_;
    bool shutdown_ = false;
};

template <typename Node>
auto process_default(Node& node,
                     NodeRequest& request) -> bool;
template <typename Node>
auto send_status(Node& node,
                 Client& client) -> void;
template <typename Node>
auto receive_traces(Node& node,
                    boost::asio::streambuf& sbuf) -> void;
template <typename Node>
auto receive_tests(Node& node,
                   boost::asio::streambuf& sbuf) -> void;
template <typename Node>
auto transmit_traces(Node& node,
                     Client& client) -> void;
template <typename Node>
auto transmit_tests(Node& node,
                    Client& client) -> void;
template <typename Node>
auto transmit_errors(Node& node,
                     Client& client) -> void;

template <typename Node>
NodeDriver<Node>::NodeDriver(const IPAddress& master_ipa,
                             const Port& master_port,
                             AtomicGuard<Node>& node) :
    node_(node),
    node_id_(node_.acquire()->id()),
    master_ip_address_(master_ipa),
    master_port_(master_port)
{
}

template <typename Node>
auto NodeDriver<Node>::operator()() -> void
{
    AsyncTask async_task{std::bind(&NodeDriver<Node>::run_listener,
                                   this)};

    run_node();
}

template <typename Node>
auto NodeDriver<Node>::run_node() -> void
{
    while(!shutdown_)
    {
        if(is_exception_thrown())
        {
            rethrow_exception();
        }

        node_.acquire()->run();
    }
}

template <typename Node>
auto NodeDriver<Node>::run_listener() -> void
{
    execute_and_catch([this]()
    {
        auto port = retrieve_own_port();

        Client client(master_ip_address_,
              std::to_string(port));

        client.connect();

        while(!shutdown_)
        {
        boost::asio::streambuf sbuf;
        auto pkinfo = client.read(sbuf);

        if(pkinfo.id != node_id_)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::network_type_mismatch{pkinfo.id}
                              << err::network_type_mismatch{node_id_});
        }

        auto request = NodeRequest{pkinfo,
                       sbuf,
                       client};

        auto processed = process(node_,
                     request);

        if(!processed)
        {
            shutdown_ = process_default(node_,
                        request);
        }

    //        display_status();
        }

    });
}

template <typename Node>
auto NodeDriver<Node>::display_status() -> void
{
    using namespace std;

    cout << setw(14) << "traces"
         << "|"
         << setw(14) << "tests"
         << endl;

    auto status = node_.acquire()->status();

    cout << setw(14) << status.trace_count
         << "|"
         << setw(14) << status.test_case_count
         << endl;
}

template <typename Node>
auto NodeDriver<Node>::retrieve_own_port() const -> Port
{
    Client client(master_ip_address_,
                  std::to_string(master_port_));

    client.connect();

    client.write(node_id_,
                 node_.acquire()->type());

    auto pkinfo = client.read();

    if(pkinfo.type != packet_type::cluster_port)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::network_type_mismatch{pkinfo.type}
                                          << err::network_type_mismatch{packet_type::cluster_port});
    }

    return pkinfo.id;
}

template <typename Node>
auto process_default(Node& node,
                     NodeRequest& request) -> bool
{
    auto shutdown = false;

    switch(request.pkinfo_.type)
    {
    case packet_type::cluster_status_request:
        send_status(node,
                    request.client_);
//        std::cout << "send_status" << std::endl;
        break;
    case packet_type::cluster_config:
        receive_config(node,
                       request.sbuf_);
        break;
    case packet_type::cluster_trace_request:
        transmit_traces(node,
                        request.client_);
        std::cout << "cluster_trace_request" << std::endl;
        break;
    case packet_type::cluster_trace:
        std::cout << "cluster_trace" << std::endl;
        receive_traces(node,
                       request.sbuf_);
        break;
    case packet_type::cluster_test_case_request:
        transmit_tests(node,
                       request.client_);
        std::cout << "cluster_test_case_request" << std::endl;
        break;
    case packet_type::cluster_test_case:
        std::cout << "cluster_test_case" << std::endl;
        receive_tests(node,
                      request.sbuf_);
        break;
    case packet_type::cluster_shutdown:
        shutdown = true;
        std::cout << "shutdown" << std::endl;
        break;
    case packet_type::cluster_commence:
        node.acquire()->commence();
        break;
    case packet_type::cluster_reset:
        node.acquire()->reset();
        break;
    case packet_type::cluster_error_log_request:
        transmit_errors(node,
                        request.client_);
        std::cout << "cluster_error_log_request" << std::endl;
        break;
    default:
        BOOST_THROW_EXCEPTION(Exception{} << err::network_type{request.pkinfo_.type});
        break;
    }

    return shutdown;
}

template <typename Node>
auto send_status(Node& node,
                 Client& client) -> void
{
    auto pkinfo = PacketInfo{node.acquire()->id(),
                             0,
                             packet_type::cluster_status};

    auto status = node.acquire()->status();

    write_serialized_binary(client,
                            pkinfo,
                            status);
}

template <typename Node>
auto receive_config(Node& node,
                    boost::asio::streambuf& sbuf) -> void
{
    std::cout << "receive_config" << std::endl;
    auto options = option::Dispatch{};

    read_serialized_binary(sbuf,
                           options);

    auto lock = node.acquire();

    lock->update(options);
}

template <typename Node>
auto receive_traces(Node& node,
                    boost::asio::streambuf& sbuf) -> void
{
    std::vector<Trace> traces;

    read_serialized_binary(sbuf,
                           traces);

    node.acquire()->push(traces);
}

template <typename Node>
auto receive_tests(Node& node,
                   boost::asio::streambuf& sbuf) -> void
{
    std::vector<TestCase> tcs;

    read_serialized_binary(sbuf,
                           tcs);

    node.acquire()->push(tcs);
}

template <typename Node>
auto transmit_traces(Node& node,
                     Client& client) -> void
{
    auto pkinfo = PacketInfo{node.acquire()->id(),
                             0,
                             packet_type::cluster_trace};

    auto traces = std::vector<Trace>{};

    {
        uint64_t size = 0u; // Warning, Clang raises an internal compiler assertion when I type: auto size = uint64_t{0u};
        auto lock = node.acquire();

        while(size < bandwidth_in_bytes &&
              !lock->traces().empty())
        {
            auto trace = lock->pop_trace();

            size += trace.data_.size();
            size += sizeof(trace.uuid_);

            traces.emplace_back(trace);
        }
    }

    write_serialized_binary(client,
                            pkinfo,
                            traces);
}

template <typename Node>
auto transmit_tests(Node& node,
                    Client& client) -> void
{
    auto pkinfo = PacketInfo{node.acquire()->id(),
                             0,
                             packet_type::cluster_test_case};

    auto tests = std::vector<TestCase>{};

    {
        uint64_t size = 0u; // Warning, Clang raises an internal compiler assertion when I type: auto size = uint64_t{0u};
        auto lock = node.acquire();

        while(size < bandwidth_in_bytes &&
              !lock->tests().empty())
        {
            auto tc = lock->pop_test();

            for(const auto& e : tc.get_elements())
            {
                size += e.data.size();
            }

            tests.emplace_back(tc);
        }
    }

    write_serialized_binary(client,
                            pkinfo,
                            tests);
}

template <typename Node>
auto transmit_errors(Node& node,
                     Client& client) -> void
{
    auto pkinfo = PacketInfo{node.acquire()->id(),
                             0,
                             packet_type::cluster_test_case};

    auto errors = std::vector<log::NodeError>{};

    {
        uint64_t size = 0u; // Warning, Clang raises an internal compiler assertion when I type: auto size = uint64_t{0u};
        auto lock = node.acquire();

        while(size < bandwidth_in_bytes &&
              !lock->errors().empty())
        {
            auto e = lock->pop_error();

            size += e.log.size();

            errors.emplace_back(e);
        }
    }

    write_serialized_binary(client,
                            pkinfo,
                            errors);
}

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_NODE_DRIVER_H
