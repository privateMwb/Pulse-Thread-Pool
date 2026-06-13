// ThreadPool Unit Test Suite
// Tests correctness of thread pool task execution and lifecycle management:
//
// - basic execution
// - future results
// - task arguments
// - move-only callable
// - pause and resume
// - shutdown finish
// - shutdown discard
// - double shutdown
// - after shutdown
// - exception tracking
// - pool statistics
// - pool state
// - stress concurrency

#include <iostream>
#include <cassert>
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <vector>
#include <future>
#include <memory>
#include <numeric>

#include "ThreadPool.h"

// Basic Execution
// verifies tasks submitted via execute() all run to completion
void basicExecution() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (std::size_t i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(counter.load() == 100);

    std::cout << "[PASS] Basic Execution\n";
}

// Future Results
// verifies enqueue() returns correct values via std::future
void futureResults() {
    ThreadPool pool(4);

    auto f1 = pool.enqueue([] { return 42; });
    auto f2 = pool.enqueue([] { return 10 * 10; });

    assert(f1.get() == 42);
    assert(f2.get() == 100);

    std::cout << "[PASS] Future Results\n";
}

// Task Arguments
// verifies enqueue() correctly forwards arguments to the task
void taskArguments() {
    ThreadPool pool(4);

    auto f1 = pool.enqueue([](int a, int b) { return a + b; }, 21, 12);
    assert(f1.get() == 33);

    auto f2 = pool.enqueue([](std::string s, int n) {
        return static_cast<int>(s.size()) + n;
    }, std::string("hello"), 5);
    assert(f2.get() == 10);

    std::cout << "[PASS] Task Arguments\n";
}

// Move-Only Callable
// verifies the queue handles move-only types via std::move_only_function
void moveOnlyCallable() {
    ThreadPool pool(2);

    auto sentinel = std::make_unique<int>(99);
    std::atomic<int> result{0};

    pool.execute([p = std::move(sentinel), &result] {
        result.store(*p, std::memory_order_relaxed);
    });

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(result.load() == 99);

    std::cout << "[PASS] Move-Only Callable\n";
}

// Pause Resume
// verifies tasks do not execute while paused and resume correctly
void pauseResume() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    pool.pause();

    for (std::size_t i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(counter.load() == 0);

    pool.resume();
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(counter.load() == 100);

    std::cout << "[PASS] Pause Resume\n";
}

// Shutdown Finish
// verifies all queued tasks complete before the pool stops
void shutdownFinish() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (std::size_t i = 0; i < 1000; ++i) {
        (void)pool.enqueue([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(counter.load() == 1000);

    std::cout << "[PASS] Shutdown Finish\n";
}

// Shutdown Discard
// verifies pending tasks are dropped when DiscardTasks mode is used
void shutdownDiscard() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (std::size_t i = 0; i < 1000; ++i) {
        pool.execute([&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown(ThreadPool::ShutdownMode::DiscardTasks);

    assert(counter.load() < 1000);

    std::cout << "[PASS] Shutdown Discard\n";
}

// Double Shutdown
// verifies calling shutdown() twice does not crash or deadlock
void doubleShutdown() {
    ThreadPool pool(4);

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(pool.isStopped());

    std::cout << "[PASS] Double Shutdown\n";
}

// After Shutdown
// verifies enqueue() and execute() throw after the pool is stopped
void afterShutdown() {
    ThreadPool pool(4);

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    bool enqueueThrew = false;
    bool executeThrew = false;

    try {
        (void)pool.enqueue([] {});
    } catch (const std::runtime_error&) {
        enqueueThrew = true;
    }

    try {
        pool.execute([] {});
    } catch (const std::runtime_error&) {
        executeThrew = true;
    }

    assert(enqueueThrew);
    assert(executeThrew);

    std::cout << "[PASS] After Shutdown\n";
}

// Exception Tracking
// verifies execute() exceptions increment exceptionCount()
// and enqueue() exceptions are forwarded through the future only
void exceptionTracking() {
    ThreadPool pool(4);

    for (std::size_t i = 0; i < 10; ++i) {
        pool.execute([] {
            throw std::runtime_error("execute exception");
        });
    }

    std::vector<std::future<void>> futures;
    for (std::size_t i = 0; i < 5; ++i) {
        futures.push_back(pool.enqueue([] {
            throw std::runtime_error("enqueue exception");
        }));
    }

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(pool.exceptionCount() == 10);

    int futureExceptions = 0;
    for (auto& f : futures) {
        try {
            f.get();
        } catch (const std::runtime_error&) {
            ++futureExceptions;
        }
    }
    assert(futureExceptions == 5);

    std::cout << "[PASS] Exception Tracking\n";
}

// Pool Statistics
// verifies activeTaskCount(), queuedTasks(), and threadCount() are accurate
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

    assert(pool.threadCount()     == 4);
    assert(pool.activeTaskCount() == 4);
    assert(pool.queuedTasks()     == 10);

    gate.store(true, std::memory_order_release);
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(pool.activeTaskCount() == 0);
    assert(pool.queuedTasks()     == 0);

    std::cout << "[PASS] Pool Statistics\n";
}

// Pool State
// verifies isPaused() and isStopped() reflect the correct state
void poolState() {
    ThreadPool pool(4);

    pool.pause();
    assert(pool.isPaused());

    pool.resume();
    assert(!pool.isPaused());
    assert(!pool.isStopped());

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    assert(pool.isStopped());

    std::cout << "[PASS] Pool State\n";
}

// Stress Concurrency
// hammers enqueue from multiple threads simultaneously to expose data races
void stressConcurrency() {
    ThreadPool pool(8);

    constexpr int total = 10'000;
    std::atomic<int> counter{0};

    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&pool, &counter] {
            for (int i = 0; i < total / 4; ++i) {
                pool.execute([&counter] {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& p : producers)
        p.join();

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    assert(counter.load() == total);

    std::cout << "[PASS] Stress Concurrency\n";
}

int main() {
    basicExecution();
    futureResults();
    taskArguments();
    moveOnlyCallable();
    pauseResume();
    shutdownFinish();
    shutdownDiscard();
    doubleShutdown();
    afterShutdown();
    exceptionTracking();
    poolStatistics();
    poolState();
    stressConcurrency();

    return 0;
}


