#ifndef CRETE_ASYNC_TASK_H
#define CRETE_ASYNC_TASK_H

#include <boost/thread.hpp>
#include <atomic>
#include <iostream>

namespace crete
{

/**
 * @brief The AsyncTask class
 *
 * Remarks:
 *
 * There seems to be considerable overlap with this and std::packaged_task.
 */
class AsyncTask
{
public:
    AsyncTask();
    template <typename Func, typename... Args>
    AsyncTask(Func f, Args&&... args);
    ~AsyncTask();

    auto operator=(AsyncTask&& other) -> AsyncTask&;

    auto is_finished() const -> bool;
    auto is_exception_thrown() const -> bool;
    [[noreturn]] auto rethrow_exception() -> void;
    auto release_exception() -> std::exception_ptr;

private:
    boost::thread thread_;
    std::atomic<bool> flag_{false};
    std::exception_ptr eptr_;
};

template <typename Func, typename... Args>
AsyncTask::AsyncTask(Func f,
                     Args&&... args)
{
    auto wrapper = [this, args...](Func f) { // TODO: ideally, this should do perfect forwarding to the wrapper.

        try // Need separate try...catch b/c this is a separate thread.
        {
            f(args...);
        }
        catch(...)
        {
            std::cerr << "exception raised" << std::endl;
            eptr_ = std::current_exception();
        }

        flag_.exchange(true, std::memory_order_seq_cst);
    };

    thread_ = boost::thread{wrapper, f};
}

inline
AsyncTask::AsyncTask()
{
    flag_.exchange(true, std::memory_order_seq_cst);
}

inline
AsyncTask::~AsyncTask()
{
    if(thread_.joinable())
    {
        thread_.join();
    }
}

inline
auto AsyncTask::is_finished() const -> bool
{
    return flag_.load(std::memory_order_seq_cst);
}

inline
auto AsyncTask::is_exception_thrown() const -> bool
{
    return static_cast<bool>(eptr_);
}

inline
auto AsyncTask::rethrow_exception() -> void
{
    std::rethrow_exception(eptr_);
}

inline
auto AsyncTask::release_exception() -> std::exception_ptr
{
    auto tmp = eptr_;
    eptr_ = std::exception_ptr{};

    return tmp;
}

} // namespace crete

#endif // CRETE_ASYNC_TASK_H
