#include <iostream>
#include <cassert>
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>

#include "ThreadPool.h"

void basic_execution() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    for(std::size_t i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            ++counter;
        });
    }
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(counter.load() == 100);
    
    std::cout << "[PASS] Basic Execution\n\n";
}

void future_results() {
    ThreadPool pool(4);
    
    auto f1 = pool.enqueue([] {
        return 42;
    });
    
    auto f2 = pool.enqueue([] {
        return 10 * 10;
    });
    
    assert(f1.get() == 42);
    assert(f2.get() == 100);
    
    std::cout << "[PASS] Future Results\n\n";
}

void task_arguments() {
    ThreadPool pool(4);
    
    auto add = [](int a, int b) {
        return a + b;
    };
    
    auto f1 = pool.enqueue(add, 21, 12);
    
    assert(f1.get() == 33);
    
    std::cout << "[PASS] Task Arguments\n\n";
}

void pause_resume() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    pool.pause();
    
    for(std::size_t i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            ++counter;
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    assert(counter.load() == 0);
    
    pool.resume();
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(counter.load() == 100);
    
    std::cout << "[PASS] Pause & Resume\n\n";
}

void shutdown_finish() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    for(std::size_t i = 0; i < 1000; ++i) {
        pool.enqueue([&counter] {
            ++counter;
        });
    }
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(counter.load() == 1000);
    
    std::cout << "[PASS] Shutdown Finish\n\n";
}

void shutdown_discard() {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    for(std::size_t i = 0; i < 1000; ++i) {
        pool.enqueue([&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ++counter;
        });
    }
    
    pool.shutdown(ThreadPool::ShutdownMode::DiscardTasks);
    
    assert(counter.load() < 1000);
    
    std::cout << "[PASS] Shutdown Discard\n\n";
}

void after_shutdown() {
    ThreadPool pool(4);
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    auto enqueue_threw = false;
    auto execute_threw = false;
    
    try {
        pool.enqueue([]{});
    } catch(const std::runtime_error&) {
        enqueue_threw = true;
    }
    
    try {
        pool.execute([]{});
    } catch(const std::runtime_error&) {
        execute_threw = true;
    }
    
    assert(enqueue_threw);
    assert(execute_threw);
    
    std::cout << "[PASS] After Shutdown\n\n";
}

void exception_tracking() {
    ThreadPool pool(4);
    
    for(std::size_t i = 0; i < 10; ++i) {
        pool.execute([] {
            throw std::runtime_error("test - boom");
        });
    }
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(pool.exceptionCount() == 10);
    
    std::cout << "[PASS] Exception Tracking\n\n";
}

void pool_statistics() {
    ThreadPool pool(4);
    
    for(std::size_t i = 0; i < 100; ++i) {
        pool.execute([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    assert(pool.threadCount() == 4);
    assert(pool.activeTaskCount() > 0);
    assert(pool.queuedTasks() > 0);
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(pool.activeTaskCount() == 0);
    
    std::cout << "[PASS] Pool Statistics\n\n";
}

void pool_state() {
    ThreadPool pool(4);
    
    pool.pause();
    
    assert(pool.isPaused());
    
    pool.resume();
    
    assert(!pool.isPaused());
    assert(!pool.isStopped());
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    
    assert(pool.isStopped());
    
    std::cout << "[PASS] Pool State\n\n";
}

int main() {
	basic_execution();
	
	future_results();
	
	task_arguments();
	
	pause_resume();
	
	shutdown_finish();
	
	shutdown_discard();
	
	after_shutdown();
	
	exception_tracking();
	
	pool_statistics();
	
	pool_state();
	
	std::cout << "All Test Passed\n";
}

