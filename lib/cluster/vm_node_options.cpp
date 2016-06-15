#include <crete/cluster/vm_node_options.h>
#include <crete/exception.h>

namespace pt = boost::property_tree;

namespace crete
{
namespace cluster
{
namespace node
{
namespace option
{

VMNode::VMNode(const pt::ptree& tree)
    : master(tree)
    , vm(tree)
{
}

VM::VM(const pt::ptree& tree)
{
    auto opt_vm = tree.get_child_optional("crete.vm");

    if(opt_vm)
    {
        auto& vme = *opt_vm;

        path.x86 = vme.get<std::string>("path.x86", path.x86);
        path.x64 = vme.get<std::string>("path.x64", path.x64);
        count = vme.get<uint32_t>("count", count);

        if(!path.x86.empty()) exception::file_exists(path.x86);
        if(!path.x64.empty()) exception::file_exists(path.x64);

        if(count == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{"crete.vm.count"});
        }
    }
}

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete
