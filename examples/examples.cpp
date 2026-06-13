// ThreadPool Examples
// Demonstrates basic usage of the thread pool:
//
// - basic task execution (fire-and-forget tasks)
// - future results (enqueue with std::future)
// - task argument forwarding
// - move-only callables (std::move_only_function support)
// - pause and resume task processing
// - shutdown modes (FinishTasks and DiscardTasks)
// - exception handling (execute and enqueue)
// - pool statistics (active tasks, queued tasks, thread count, exceptions)
//
// These examples illustrate the core features and intended usage of ThreadPool.

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <thread>

#include "ThreadPool.h"

// Basic Execution
// shows fire-and-forget task submission using execute()
void basicExecution() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (std::size_t i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    // counter == 100
}

// Future Results
// shows enqueue() returning values via std::future
void futureResults() {
    ThreadPool pool(4);

    auto f1 = pool.enqueue([] { return 42; });
    auto f2 = pool.enqueue([] { return 10 * 10; });

    int r1 = f1.get(); // 42
    int r2 = f2.get(); // 100

    (void)r1; (void)r2;
}

// Task Arguments
// shows enqueue() forwarding arguments to the task
void taskArguments() {
    ThreadPool pool(4);

    auto f1 = pool.enqueue([](int a, int b) { return a + b; }, 21, 12);
    auto f2 = pool.enqueue([](std::string s, int n) {
        return static_cast<int>(s.size()) + n;
    }, std::string("hello"), 5);

    int r1 = f1.get(); // 33
    int r2 = f2.get(); // 10

    (void)r1; (void)r2;
}

// Move-Only Callable
// shows that execute() accepts move-only types via std::move_only_function
void moveOnlyCallable() {
    ThreadPool pool(2);

    auto sentinel = std::make_unique<int>(99);
    std::atomic<int> result{0};

    pool.execute([p = std::move(sentinel), &result] {
        result.store(*p, std::memory_order_relaxed);
    });

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    // result == 99
}

// Pause Resume
// shows pausing the pool, queuing tasks, then resuming execution
void pauseResume() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    pool.pause();

    for (std::size_t i = 0; i < 10; ++i) {
        pool.execute([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // counter == 0 — tasks are queued but not executing

    pool.resume();
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    // counter == 10
    (void)counter;
}

// Shutdown
// shows FinishTasks draining the queue and DiscardTasks dropping pending work
void shutdown() {
    // FinishTasks — all queued tasks complete before stopping
    {
        ThreadPool pool(4);

        std::atomic<int> counter{0};

        for (std::size_t i = 0; i < 100; ++i) {
            pool.execute([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
        // counter == 100
    }

    // DiscardTasks — pending tasks are dropped immediately
    {
        ThreadPool pool(4);

        std::atomic<int> counter{0};

        for (std::size_t i = 0; i < 1000; ++i) {
            pool.execute([&counter] {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.shutdown(ThreadPool::ShutdownMode::DiscardTasks);
        // counter < 1000
        (void)counter;
    }
}

// Exceptions
// shows execute() exceptions tracked via exceptionCount()
// and enqueue() exceptions forwarded through the future
void exceptions() {
    ThreadPool pool(4);

    // execute — exception is caught internally and counted
    pool.execute([] {
        throw std::runtime_error("execute exception");
    });

    // enqueue — exception is forwarded through the future
    auto f = pool.enqueue([] {
        throw std::runtime_error("enqueue exception");
    });

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    std::size_t count = pool.exceptionCount(); // 1
    (void)count;

    try {
        f.get();
    } catch (const std::runtime_error&) {
        // exception from enqueue forwarded here
    }
}

// Pool Statistics
// shows activeTaskCount(), queuedTasks(), threadCount(), and exceptionCount()
void poolStatistics() {
    ThreadPool pool(4);

    std::atomic<bool> gate{false};
    std::atomic<int>  ready{0};

    for (std::size_t i = 0; i < 4; ++i) {
        pool.execute([&gate, &ready] {
            ready.fetch_add(1, std::memory_order_release);
            while (!gate.load(std::memory_order_acquire))
                std::this_thread::yield();
        });
    }

    for (std::size_t i = 0; i < 10; ++i) {
        pool.execute([&gate] {
            while (!gate.load(std::memory_order_acquire))
                std::this_thread::yield();
        });
    }

    while (ready.load(std::memory_order_acquire) < 4)
        std::this_thread::yield();

    std::size_t active  = pool.activeTaskCount(); // 4
    std::size_t queued  = pool.queuedTasks();     // 10
    std::size_t threads = pool.threadCount();     // 4

    (void)active; (void)queued; (void)threads;

    gate.store(true, std::memory_order_release);
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
}

int main() {
    basicExecution();
    futureResults();
    taskArguments();
    moveOnlyCallable();
    pauseResume();
    shutdown();
    exceptions();
    poolStatistics();

    return 0;
}
