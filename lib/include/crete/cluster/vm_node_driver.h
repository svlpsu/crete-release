#ifndef CRETE_CLUSTER_VM_NODE_DRIVER_H
#define CRETE_CLUSTER_VM_NODE_DRIVER_H

#include <crete/cluster/node_driver.h>

namespace crete
{
namespace cluster
{

template <typename Node>
class VMNodeDriver : public NodeDriver<Node>
{
public:
    VMNodeDriver(const IPAddress& master_ipa,
                 const Port& master_port,
                 AtomicGuard<Node>& node);

private:
};

template <typename Node>
VMNodeDriver<Node>::VMNodeDriver(const IPAddress& master_ipa,
                                 const Port& master_port,
                                 AtomicGuard<Node>& node) :
    NodeDriver{master_ipa,
               master_port,
               node}
{
}

template <typename Node>
auto NodeDriver<Node>::operator()() -> void
{
    boost::thread node_thread{boost::bind(&NodeDriver<Node>::run_listener,
                                          this)};

    run_node();

    node_thread.join();
}

template <typename Node>
auto NodeDriver<Node>::run_node() -> void
{
    while(!shutdown_)
    {
        node_.acquire()->run();
    }
}

template <typename Node>
auto NodeDriver<Node>::run(std::::exception_ptr eptr) -> void
{
    auto port = retrieve_own_port();

    Client client(master_ip_address_,
                  std::to_string(port));

    client.connect();

    while(!shutdown_)
    {
        boost::asio::streambuf sbuf;
        auto request = client.read(sbuf);

        if(request.id != node_id_)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::network_type_mismatch{request.id});
        }

        switch(request.type)
        {
        case packet_type::cluster_status_request:
            send_status(client);
            std::cout << "send_status" << std::endl;
            break;
        case packet_type::cluster_trace_request:
            transmit_traces(client);
            std::cout << "cluster_trace_request" << std::endl;
            break;
        case packet_type::cluster_trace:
            std::cout << "cluster_trace" << std::endl;
            receive_traces(sbuf);
            break;
        case packet_type::cluster_test_case_request:
            transmit_tests(client);
            std::cout << "cluster_test_case_request" << std::endl;
            break;
        case packet_type::cluster_test_case:
            std::cout << "cluster_test_case" << std::endl;
            receive_tests(sbuf);
            break;
        case packet_type::cluster_shutdown:
            shutdown_ = true;
            std::cout << "shutdown" << std::endl;
            break;
        }

        display_status();
    }
}

template <typename Node>
auto NodeDriver<Node>::send_status(Client& client) const -> void
{
    auto pkinfo = PacketInfo{node_id_,
                             0,
                             packet_type::cluster_status};

    auto status = node_.acquire()->status();

    write_serialized_binary(client,
                            pkinfo,
                            status);
}

template <typename Node>
auto NodeDriver<Node>::transmit_traces(Client& client) -> void
{
    auto pkinfo = PacketInfo{node_id_,
                             0,
                             packet_type::cluster_trace};

    auto traces = std::vector<Trace>{};

    {
        auto size = 0u;
        auto lock = node_.acquire();

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
auto NodeDriver<Node>::transmit_tests(Client& client) -> void
{
    auto pkinfo = PacketInfo{node_id_,
                             0,
                             packet_type::cluster_test_case};

    auto tests = std::vector<TestCase>{};

    {
        auto size = 0u;
        auto lock = node_.acquire();

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
        BOOST_THROW_EXCEPTION(Exception{} << err::network_type_mismatch{pkinfo.type});
    }

    return pkinfo.id;
}

template <typename Node>
auto NodeDriver<Node>::receive_traces(boost::asio::streambuf& sbuf) -> void
{
    std::vector<Trace> traces;

    read_serialized_binary(sbuf,
                           traces);

    node_.acquire()->push(traces);
}

template <typename Node>
auto NodeDriver<Node>::receive_tests(boost::asio::streambuf& sbuf) -> void
{
    std::vector<TestCase> tcs;

    read_serialized_binary(sbuf,
                           tcs);

    node_.acquire()->push(tcs);
}

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_VM_NODE_DRIVER_H
