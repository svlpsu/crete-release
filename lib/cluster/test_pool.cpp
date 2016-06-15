#include <crete/cluster/test_pool.h>

#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <crete/exception.h>

namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{

TestPool::TestPool(const fs::path& root)
    : random_engine_{std::time(0)}
    , root_{root}
{
}

auto TestPool::next() -> boost::optional<TestCase>
{
    if(next_.empty())
    {
        return boost::optional<TestCase>{};
    }

    // FIFO:
    auto tc = next_.back();
    next_.pop_back();

    return boost::optional<TestCase>{tc};

    // Random:
//    std::uniform_int_distribution<size_t> dist{0,
//                                               next_.size() - 1};

//    auto it = next_.begin();
//    std::advance(it,
//                 dist(random_engine_));
//    auto tc = boost::optional<TestCase>{*it};

//    next_.erase(it);

//    return tc;
}

auto TestPool::insert(const TestCase& tc) -> bool
{
    if(all_.find(tc) == all_.end()) // TODO: inefficient. Rather, if(all_.insert(*it).second) { next_.insert(*it); ... }
    {
        all_.insert(tc);
        next_.push_front(tc);

        write_test_case(tc);

        return true;
    }

    return false;
}

auto TestPool::insert(const std::vector<TestCase>& tcs) -> void
{
    for(const auto& tc : tcs)
    {
        insert(tc);
    }
}

auto TestPool::clear() -> void
{
    next_.clear();
    all_.clear();
}

auto TestPool::write_test_case(const TestCase& tc) -> void
{
    namespace fs = boost::filesystem;

    auto tc_root = root_ / "test-case";

    if(!fs::exists(tc_root))
        fs::create_directories(tc_root);

    auto name = std::to_string(all_.size());
    auto path = tc_root / name;

    fs::ofstream ofs(path,
                     std::ios_base::out | std::ios_base::binary);

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{path.string()});
    }

    tc.write(ofs);
}

auto TestPool::count_all() const -> size_t
{
    return all_.size();
}

auto TestPool::count_next() const -> size_t
{
    return next_.size();
}

bool TestCaseSetCompare::operator()(const crete::TestCase &rhs, const crete::TestCase &lhs)
{
    std::stringstream ssr, ssl;
    rhs.write(ssr);
    lhs.write(ssl);

    return ssr.str() < ssl.str();
}

} // namespace cluster
} // namespace crete
