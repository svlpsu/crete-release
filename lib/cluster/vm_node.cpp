#include <crete/cluster/vm_node.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/server.h>
#include <crete/run_config.h>
#include <crete/serialize.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>

#include <boost/process.hpp>

#include <memory>

#include "vm_node_fsm.cpp" // Unfortunate workaround to accommodate Boost.MSM.

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace crete
{
namespace cluster
{

VMNode::VMNode(const node::option::VMNode& node_options,
               const fs::path& pwd)
    : Node{packet_type::cluster_request_vm_node}
    , node_options_{node_options}
    , pwd_{pwd}
{
    if(node_options_.vm.count < 1)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_uint(node_options_.vm.count));
    }

    init_image_info();
    add_instances(node_options_.vm.count);
}

auto VMNode::run() -> void
{
    if(!commenced())
    {
        return;
    }

    poll();
}

auto VMNode::poll() -> void
{
    using namespace node::vm;

    auto any_active = false;

    for(auto& vm : vms_)
    {
        if(vm->is_flag_active<flag::trace_ready>())
        {
            push(vm->trace());

            vm->process_event(ev::trace_queued{});
        }        
        else if(vm->is_flag_active<flag::next_test>())
        {
            auto has_tests = !tests().empty();

            if(has_tests)
            {
                using boost::msm::back::HANDLED_TRUE;

                auto t = pop_test();

                if(HANDLED_TRUE != vm->process_event(ev::next_test{t}))
                {
                    push(t);
                }
            }
        }
        else if(vm->is_flag_active<flag::first_vm>())
        {
            if(master_options().vm.initial_tc.get_elements().size() > 0)
            {
                push(master_options().vm.initial_tc);
            }
            else
            {
                std::cout << "vm->is_flag_active<flag::first_vm>()" << std::endl;
                const auto& initial = vm->initial_test();

                std::cout << "vm->is_flag_active<flag::first_vm>(): tc.elem.size: "
                          << TestCase{initial}.get_elements().size()
                          << std::endl;

                push(initial);
            }

            vm->process_event(ev::poll{});
        }
        else if(vm->is_flag_active<flag::error>())
        {
            using node::vm::fsm::QemuFSM;

            push(vm->error());

            std::cerr << "pushing error!\n";

            auto pwd = vm->pwd();

            vm.reset(new QemuFSM{}); // TODO: may leak. Can I do vm = std::make_shared<QemuFSM>()?

            vm->start();

            ev::start start_ev{
                master_options(),
                node_options_,
                pwd,
                image_path(),
                false,
                target_
            };

            vm->process_event(start_ev);
        }
        else if(vm->is_flag_active<flag::terminated>())
        {
            // Do nothing.
        }
        else
        {
            vm->process_event(ev::poll{});
        }

        any_active = any_active
                     || (!vm->is_flag_active<flag::next_test>()
                         && !vm->is_flag_active<flag::terminated>());
    }

    active(any_active);
}

auto VMNode::start_FSMs() -> void
{
    using namespace node::vm;

    auto vm_path = pwd_.string();

    if(master_options().mode.distributed)
    {
        vm_path = vm_inst_pwd.string();
    }

    ev::start start_ev{
        master_options(),
        node_options_,
        vm_path,
        image_path(),
        false,
        target_
    };

    auto vm_num = 1u;

    for(auto& fsm : vms_)
    {
        fsm->start();

        auto s = start_ev;

        if(vm_num == 1)
        {
            s.first_vm_ = true;
        }

        if(master_options().mode.distributed)
        {
            s.vm_dir_ /= std::to_string(vm_num++);
        }

        fsm->process_event(s);
    }
}

auto VMNode::init_image_info() -> void
{
    auto infop = pwd_ / node_image_dir / image_info_name;

    if(fs::exists(infop))
    {

        {
            auto imagep = pwd_ / node_image_dir / image_name;

            if(!fs::exists(imagep))
            {
                fs::remove(infop);

                return;
            }
        }

        fs::ifstream ifs{infop};

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{infop.string()});
        }

        serialize::read_text(ifs,
                             image_info_);
    }
}

auto VMNode::update(const ImageInfo& ii) -> void
{
    image_info_ = ii;
    auto img_dir = pwd_ / node_image_dir;
    auto infop = img_dir / image_info_name;
    
    if(!fs::exists(img_dir))
    {
        fs::create_directories(img_dir);
    }
    
    fs::ofstream ofs{infop};

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{infop.string()});
    }
    
    serialize::write_text(ofs,
                          image_info_);
}

auto VMNode::image_info() -> const ImageInfo&
{
    return image_info_;
}

auto VMNode::image_path() -> fs::path
{
    return pwd_ / node_image_dir / image_name;
}

auto VMNode::reset() -> void
{
    using namespace node::vm;

    std::cerr << "reset()" << std::endl;

    if(!commenced())
    {
        return;
    }

    auto count = vms_.size();

    for(auto& vm : vms_)
    {
        vm->process_event(ev::terminate{});
    }

    Node::reset();

    vms_.clear();

    add_instances(count);
}

auto VMNode::target(const std::string& target) -> void
{
    target_ = target;
}

auto process(AtomicGuard<VMNode>& node,
             NodeRequest& request) -> bool
{
    switch(request.pkinfo_.type)
    {
    case packet_type::cluster_request_config:
        transmit_config(node,
                        request.client_);
        return true;
    case packet_type::cluster_image_info_request:
        transmit_image_info(node,
                            request.client_);
        return true;
    case packet_type::cluster_image_info:
        receive_image_info(node,
                           request.sbuf_);
        return true;
    case packet_type::cluster_image:
        receive_image(node,
                      request.sbuf_);
        return true;
    case packet_type::cluster_reset:
        node.acquire()->reset();
        return true;
    case packet_type::cluster_next_target:
        receive_target(node,
                       request.sbuf_);
        return true;
    case packet_type::cluster_commence:
    {
        auto lock = node.acquire();

        lock->start_FSMs(); // Once we have the options from Dispatch, we can start the VM instances.
        lock->commence();

        return true;
    }
    }

    return false;
}

auto receive_config(AtomicGuard<VMNode>& node,
                    boost::asio::streambuf& sbuf) -> void
{

}

auto transmit_config(AtomicGuard<VMNode>& node,
                     Client& client) -> void
{
    // TODO: what data type is 'config'?
    assert(0 && "TODO!");
}

auto transmit_image_info(AtomicGuard<VMNode>& node,
                           Client& client) -> void
{
    auto pkinfo = PacketInfo{0,0,0};
    auto ii = ImageInfo{};

    {
        auto lock = node.acquire();

        pkinfo.id = lock->id();
        pkinfo.type = packet_type::cluster_image_info;

        ii = lock->image_info();
    }

    write_serialized_binary(client,
                            pkinfo,
                            ii);
}

auto receive_image_info(AtomicGuard<VMNode>& node,
                        boost::asio::streambuf& sbuf) -> void
{
    std::cout << "receive_image_info" << std::endl;
    auto ii = ImageInfo{};

    read_serialized_binary(sbuf,
                           ii);

    node.acquire()->update(ii);
}

auto receive_image(AtomicGuard<VMNode>& node,
                   boost::asio::streambuf& sbuf) -> void
{
    auto image = OSImage{};
    auto image_path = node.acquire()->image_path();

    std::cout << "receive_image: " << image_path.string() << std::endl;

    read_serialized_binary(sbuf,
                           image);

    if(!fs::exists(image_path.parent_path()))
    {
        fs::create_directories(image_path.parent_path());
    }

    if(fs::exists(image_path))
    {
        fs::remove(image_path);
    }

    to_file(image,
            image_path);

    auto untared_image_name = image_path.parent_path() / node.acquire()->image_info().file_name_;

    if(!fs::exists(untared_image_name))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{untared_image_name.string()});
    }

    fs::rename(untared_image_name,
               image_path);

    std::cout << "receive_image: to_file" << std::endl;
}

auto receive_target(AtomicGuard<VMNode>& node,
                    boost::asio::streambuf& sbuf) -> void
{
    auto target = std::string{};

    read_serialized_binary(sbuf,
                           target);

    node.acquire()->target(target);
}

auto VMNode::add_instance() -> void
{
    vms_.emplace_back(std::make_shared<node::vm::fsm::QemuFSM>());
}

auto VMNode::add_instances(size_t count) -> void
{
    for(const auto& i : boost::irange(size_t(0), count))
    {
        (void)i;

        add_instance();
    }
}

} // namespace cluster
} // namespace crete
