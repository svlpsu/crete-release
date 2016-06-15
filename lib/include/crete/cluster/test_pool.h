#ifndef CRETE_TEST_POOL_H_
#define CRETE_TEST_POOL_H_

#include <set>
#include <string>
#include <vector>
#include <deque>
#include <stdint.h>
#include <random>

#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>

#include <crete/test_case.h>

namespace crete
{
namespace cluster
{

// TODO: naive comparison function to ignore redundant test cases.
// TODO: inefficient. Converts to string and compares the strings. It would be much faster to
//       hash the test case once and store it in an unordered_set.
struct TestCaseSetCompare
{
    bool operator()(const crete::TestCase& rhs, const crete::TestCase& lhs);
};

class TestPool
{
public:
    using TracePath = boost::filesystem::path;
    using TestSet = std::set<crete::TestCase, TestCaseSetCompare>;
    using TestQueue = std::deque<crete::TestCase>;

public:
    TestPool(const boost::filesystem::path& root);

    auto next() -> boost::optional<TestCase>;
    auto insert(const TestCase& tc) -> bool;
    auto insert(const std::vector<TestCase>& tcs) -> void;
    auto clear() -> void;
    auto count_all() const -> size_t;
    auto count_next() const -> size_t;
    auto write_test_case(const TestCase& tc) -> void;

private:
    TestSet all_;
    TestQueue next_;
    std::mt19937 random_engine_; // TODO: currently unused in favor of FIFO; however, should random be optional?
    boost::filesystem::path root_;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_TEST_POOL_H_
