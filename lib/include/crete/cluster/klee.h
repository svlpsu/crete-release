#ifndef CRETE_CLUSTER_KLEE_H
#define CRETE_CLUSTER_KLEE_H

#include <crete/cluster/common.h>
#include <crete/test_case.h>
#include <crete/atomic_guard.h>

#include <boost/thread.hpp>

#include <vector>

namespace crete
{
namespace cluster
{

class KleeExecutor;

/**
 * @brief The Klee class
 *
 * Intended usage: single-use disposable:
 * 1. Construct.
 * 2. Execute.
 * 3. Retreive test cases.
 * 4. Destroy.
 *
 * Invariants:
 * 	-is_finished() returns true only after execute is run.
 *  -test_cases() returns only test cases generated via symbolic execution.
 *  -concolic execution precedes symbolic execution.
 */
class Klee
{
public:
    using TestCases = std::vector<TestCase>;

public:
    Klee(const Trace& trace,
         const boost::filesystem::path& working_dir);
    ~Klee();

    auto execute_concolic() -> void;
    auto execute_symbolic() -> void;

    auto is_concolic_started() const -> bool;
    auto is_symbolic_started() const -> bool;
    auto is_concolic_finished() const -> bool;
    auto is_symbolic_finished() const -> bool;

    auto retrieve_result() -> void;

    auto test_cases() -> const TestCases&;

private:
    auto prepare() -> void; // Assumes translation already complete.

    Trace trace_;
    pid_t pid_concolic_{-1};
    pid_t pid_symbolic_{-1};
    TestCases test_cases_;
    bool is_concolic_started_{false};
    bool is_symbolic_started_{false};
    boost::filesystem::path working_dir_;
};

/**
 * @brief The KleeExecutor class
 *
 * Invariants:
 * 'is_executing' will be true until execution has completely finished and all
 * test cases have been accumulated into 'tcs'.
 */
class KleeExecutor
{
public:
    KleeExecutor(AtomicGuard<Klee>& klee,
                 const Trace& trace);


private:
    AtomicGuard<Klee>& klee_;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_KLEE_H
