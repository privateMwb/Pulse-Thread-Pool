#include <future>
#include <memory>
#include <tuple>
#include <utility>
#include <type_traits>
#include <mutex>
#include <stdexcept>

// Task Submission
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    auto taskPtr =
        std::make_shared<std::packaged_task<ReturnType()>>(
            [func      = std::forward<F>(f),
             argsTuple = std::make_tuple(std::forward<Args>(args)...)]
            mutable {
                return std::apply(func, std::move(argsTuple));
            }
        );

    auto future = taskPtr->get_future();

    {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stopFlag.load(std::memory_order_relaxed)) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        taskQueue.push([taskPtr] { (*taskPtr)(); });
    }

    condition.notify_one();

    return future;
}

// Task Execution
template<typename F>
void ThreadPool::execute(F&& task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stopFlag.load(std::memory_order_relaxed)) {
            throw std::runtime_error("execute on stopped ThreadPool");
        }

        taskQueue.emplace(std::forward<F>(task));
    }

    condition.notify_one();
}
