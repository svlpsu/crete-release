#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>

#include <crete/cluster/svm_node_options.h>
#include <crete/exception.h>

namespace pt = boost::property_tree;
namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{
namespace node
{
namespace option
{

SVMNode::SVMNode(const pt::ptree& tree)
    : master(tree)
    , svm(tree)
    , translator(tree)
{
}

SVM::SVM(const pt::ptree& tree)
{
    auto opt_svm = tree.get_child_optional("crete.svm");

    if(opt_svm)
    {
        auto& svm = *opt_svm;

        path.concolic = svm.get<std::string>("path.concolic", path.concolic);
        path.symbolic = svm.get<std::string>("path.symbolic", path.symbolic);
        count = svm.get<uint32_t>("count", count);

        if(!path.concolic.empty()) exception::file_exists(path.concolic);
        if(!path.symbolic.empty()) exception::file_exists(path.symbolic);

        if(count == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{"crete.svm.count"});
        }
    }
}

Translator::Translator(const pt::ptree& tree)
{
    auto opt_trans = tree.get_child_optional("crete.translator");

    if(opt_trans)
    {
        auto& trans = *opt_trans;

        path.x86 = trans.get<std::string>("path.x86", path.x86);
        path.x64 = trans.get<std::string>("path.x64", path.x64);

        if(!path.x86.empty()) exception::file_exists(path.x86);
        if(!path.x64.empty()) exception::file_exists(path.x64);
    }
}

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete
