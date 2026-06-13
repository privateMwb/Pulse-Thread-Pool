#include "ThreadPool.h"

// Constructors & Destructor
ThreadPool::ThreadPool(std::size_t threadCount)
    : stopFlag(false)
    , paused(false)
    , shutdownMode(ShutdownMode::FinishTasks)
    , activeTasks(0)
    , exceptionCounter(0)
{
    workers.reserve(threadCount);

    for (std::size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back([this] {
            while (true) {
                MoveOnlyTask task;

                {
                    std::unique_lock<std::mutex> lock(queueMutex);

                    condition.wait(lock, [this] {
                        return stopFlag.load(std::memory_order_relaxed)
                            || (!paused.load(std::memory_order_relaxed)
                                && !taskQueue.empty());
                    });

                    if (stopFlag.load(std::memory_order_relaxed)) {
                        if (shutdownMode == ShutdownMode::DiscardTasks)
                            return;

                        if (taskQueue.empty())
                            return;
                    }

                    task = std::move(taskQueue.front());
                    taskQueue.pop();
                }

                ActiveTaskGuard guard(activeTasks);

                try {
                    task();
                } catch (...) {
                    exceptionCounter.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown(shutdownMode);
}

// Control
void ThreadPool::pause() noexcept {
    paused.store(true, std::memory_order_relaxed);
}

void ThreadPool::resume() noexcept {
    paused.store(false, std::memory_order_relaxed);
    condition.notify_all();
}

void ThreadPool::shutdown(ShutdownMode mode) noexcept {
    {
        std::lock_guard<std::mutex> lock(queueMutex);

        if (stopFlag.load(std::memory_order_relaxed))
            return;

        shutdownMode = mode;
        stopFlag.store(true, std::memory_order_relaxed);
    }

    condition.notify_all();

    for (std::thread& worker : workers)
        if (worker.joinable())
            worker.join();
}

// Introspection
std::size_t ThreadPool::activeTaskCount() const noexcept {
    return activeTasks.load(std::memory_order_relaxed);
}

std::size_t ThreadPool::queuedTasks() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
}

std::size_t ThreadPool::threadCount() const noexcept {
    return workers.size();
}

std::size_t ThreadPool::exceptionCount() const noexcept {
    return exceptionCounter.load(std::memory_order_relaxed);
}

// State
bool ThreadPool::isPaused() const noexcept {
    return paused.load(std::memory_order_relaxed);
}

bool ThreadPool::isStopped() const noexcept {
    return stopFlag.load(std::memory_order_relaxed);
}
