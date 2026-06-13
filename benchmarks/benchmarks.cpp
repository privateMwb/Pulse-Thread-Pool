// ThreadPool Benchmark Suite
// Measures performance of ThreadPool against std::async and raw std::thread:
//
// - task throughput (raw submission and execution speed)
// - future latency (round-trip enqueue + future.get() cost)
// - mixed workload (short, medium, and long tasks)
// - contention (multiple producers hammering enqueue simultaneously)

#include <iostream>
#include <cstddef>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <atomic>
#include <mutex>

#include "ThreadPool.h"
#include "utils/Table.h"

// Config
static constexpr std::size_t THREAD_COUNT            = 8;
static constexpr std::size_t THROUGHPUT_TASKS        = 100'000;
static constexpr std::size_t THROUGHPUT_TASKS_RAW    = 1'000;
static constexpr std::size_t LATENCY_TASKS           = 1'000;
static constexpr std::size_t MIXED_TASKS             = 10'000;
static constexpr std::size_t MIXED_TASKS_RAW         = 1'000;
static constexpr std::size_t CONTENTION_TASKS        = 100'000;
static constexpr std::size_t CONTENTION_TASKS_RAW    = 4'000;   // 1000 per producer
static constexpr std::size_t CONTENTION_PRODUCERS    = 4;

// returns elapsed microseconds for a callable
template<typename F>
auto duration(F func) {
	auto start = std::chrono::steady_clock::now();
	func();
	auto end   = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
}

// Task Throughput
// measures raw submission and execution speed of N fire-and-forget tasks
void taskThroughput() {
	// ThreadPool
	auto poolTime = duration([&] {
		ThreadPool pool(THREAD_COUNT);

		std::atomic<int> counter{0};

		for (std::size_t i = 0; i < THROUGHPUT_TASKS; ++i) {
			pool.execute([&counter] {
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
	});

	// std::async
	auto asyncTime = duration([&] {
		std::vector<std::future<void>> futures;
		futures.reserve(THROUGHPUT_TASKS_RAW);

		std::atomic<int> counter{0};

		for (std::size_t i = 0; i < THROUGHPUT_TASKS_RAW; ++i) {
			futures.push_back(std::async(std::launch::async, [&counter] {
				counter.fetch_add(1, std::memory_order_relaxed);
			}));
		}

		for (auto& f : futures) f.get();
	});

	// Raw std::thread
	auto threadTime = duration([&] {
		std::vector<std::thread> threads;
		threads.reserve(THROUGHPUT_TASKS_RAW);

		std::atomic<int> counter{0};

		for (std::size_t i = 0; i < THROUGHPUT_TASKS_RAW; ++i) {
			threads.emplace_back([&counter] {
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		for (auto& t : threads) t.join();
	});

	std::vector<std::string>              headers{ "Method", "Time (us)" };
	std::vector<std::vector<std::string>> data{
		{ "ThreadPool", "std::async", "std::thread" },
		{
			Table::format(poolTime.count(),   "us"),
			Table::format(asyncTime.count(),  "us"),
			Table::format(threadTime.count(), "us"),
		}
	};

	Table::table(
	    "Task Throughput  (Pool: " + std::to_string(THROUGHPUT_TASKS)
	    + "  /  async+thread: " + std::to_string(THROUGHPUT_TASKS_RAW) + " tasks)",
	    headers, data, 60);
}

// Future Latency
// measures round-trip cost of enqueue() + future.get()
void futureLatency() {
	// ThreadPool
	auto poolTime = duration([&] {
		ThreadPool pool(THREAD_COUNT);

		for (std::size_t i = 0; i < LATENCY_TASKS; ++i) {
			auto f = pool.enqueue([] { return 1; });
			f.get();
		}
	});

	// std::async
	auto asyncTime = duration([&] {
		for (std::size_t i = 0; i < LATENCY_TASKS; ++i) {
			auto f = std::async(std::launch::async, [] { return 1; });
			f.get();
		}
	});

	// Raw std::thread + promise
	auto threadTime = duration([&] {
		for (std::size_t i = 0; i < LATENCY_TASKS; ++i) {
			std::promise<int> p;
			auto f = p.get_future();
			std::thread t([&p] { p.set_value(1); });
			f.get();
			t.join();
		}
	});

	std::vector<std::string>              headers{ "Method", "Time (us)" };
	std::vector<std::vector<std::string>> data{
		{ "ThreadPool", "std::async", "std::thread" },
		{
			Table::format(poolTime.count(),   "us"),
			Table::format(asyncTime.count(),  "us"),
			Table::format(threadTime.count(), "us"),
		}
	};

	Table::table(
	    "Future Latency  (" + std::to_string(LATENCY_TASKS) + " round-trips)",
	    headers, data, 44);
}

// Mixed Workload
// measures performance across short, medium, and long tasks
void mixedWorkload() {
	auto work = [](std::size_t iterations) {
		volatile std::size_t x = 0;
		for (std::size_t i = 0; i < iterations; ++i) x += i;
	};

	// ThreadPool
	auto poolTime = duration([&] {
		ThreadPool pool(THREAD_COUNT);

		for (std::size_t i = 0; i < MIXED_TASKS; ++i) {
			switch (i % 3) {
			case 0:
				pool.execute([&work] { work(10);    });
				break;
			case 1:
				pool.execute([&work] { work(1000);  });
				break;
			case 2:
				pool.execute([&work] { work(10000); });
				break;
			}
		}

		pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
	});

	// std::async
	auto asyncTime = duration([&] {
		std::vector<std::future<void>> futures;
		futures.reserve(MIXED_TASKS_RAW);

		for (std::size_t i = 0; i < MIXED_TASKS_RAW; ++i) {
			switch (i % 3) {
			case 0:
				futures.push_back(std::async(std::launch::async, [&work] { work(10);    }));
				break;
			case 1:
				futures.push_back(std::async(std::launch::async, [&work] { work(1000);  }));
				break;
			case 2:
				futures.push_back(std::async(std::launch::async, [&work] { work(10000); }));
				break;
			}
		}

		for (auto& f : futures) f.get();
	});

	// Raw std::thread
	auto threadTime = duration([&] {
		std::vector<std::thread> threads;
		threads.reserve(MIXED_TASKS_RAW);

		for (std::size_t i = 0; i < MIXED_TASKS_RAW; ++i) {
			switch (i % 3) {
			case 0:
				threads.emplace_back([&work] { work(10);    });
				break;
			case 1:
				threads.emplace_back([&work] { work(1000);  });
				break;
			case 2:
				threads.emplace_back([&work] { work(10000); });
				break;
			}
		}

		for (auto& t : threads) t.join();
	});

	std::vector<std::string>              headers{ "Method", "Time (us)" };
	std::vector<std::vector<std::string>> data{
		{ "ThreadPool", "std::async", "std::thread" },
		{
			Table::format(poolTime.count(),   "us"),
			Table::format(asyncTime.count(),  "us"),
			Table::format(threadTime.count(), "us"),
		}
	};

	Table::table(
	    "Mixed Workload  (Pool: " + std::to_string(MIXED_TASKS)
	    + "  /  async+thread: " + std::to_string(MIXED_TASKS_RAW) + " tasks)",
	    headers, data, 60);
}

// Contention
// measures enqueue performance under simultaneous pressure from multiple producers
void contention() {
    static constexpr std::size_t tasksPerProducer    = CONTENTION_TASKS / CONTENTION_PRODUCERS;
    static constexpr std::size_t tasksPerProducerRaw = CONTENTION_TASKS_RAW / CONTENTION_PRODUCERS;

    // ThreadPool
    auto poolTime = duration([&] {
        ThreadPool pool(THREAD_COUNT);

        std::atomic<int> counter{0};
        std::vector<std::thread> producers;
        producers.reserve(CONTENTION_PRODUCERS);

        for (std::size_t t = 0; t < CONTENTION_PRODUCERS; ++t) {
            producers.emplace_back([&pool, &counter] {
                for (std::size_t i = 0; i < tasksPerProducer; ++i) {
                    pool.execute([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            });
        }

        for (auto& p : producers) p.join();
        pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
    });

    // std::async
    auto asyncTime = duration([&] {
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        futures.reserve(CONTENTION_TASKS_RAW);
        std::mutex futuresMutex;

        std::vector<std::thread> producers;
        producers.reserve(CONTENTION_PRODUCERS);

        for (std::size_t t = 0; t < CONTENTION_PRODUCERS; ++t) {
            producers.emplace_back([&] {
                for (std::size_t i = 0; i < tasksPerProducerRaw; ++i) {
                    auto f = std::async(std::launch::async, [&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                    std::lock_guard<std::mutex> lock(futuresMutex);
                    futures.push_back(std::move(f));
                }
            });
        }

        for (auto& p : producers) p.join();
        for (auto& f : futures)   f.get();
    });

    // Raw std::thread
    auto threadTime = duration([&] {
        std::atomic<int> counter{0};
        std::vector<std::thread> allThreads;
        allThreads.reserve(CONTENTION_TASKS_RAW);
        std::mutex threadsMutex;

        std::vector<std::thread> producers;
        producers.reserve(CONTENTION_PRODUCERS);

        for (std::size_t t = 0; t < CONTENTION_PRODUCERS; ++t) {
            producers.emplace_back([&] {
                for (std::size_t i = 0; i < tasksPerProducerRaw; ++i) {
                    std::thread worker([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                    std::lock_guard<std::mutex> lock(threadsMutex);
                    allThreads.push_back(std::move(worker));
                }
            });
        }

        for (auto& p : producers)  p.join();
        for (auto& t : allThreads) t.join();
    });

    std::vector<std::string>              headers{ "Method", "Time (us)" };
    std::vector<std::vector<std::string>> data{
        { "ThreadPool", "std::async", "std::thread" },
        {
            Table::format(poolTime.count(),   "us"),
            Table::format(asyncTime.count(),  "us"),
            Table::format(threadTime.count(), "us"),
        }
    };

    Table::table(
        "Contention  (Pool: " + std::to_string(CONTENTION_TASKS)
            + "  /  async+thread: " + std::to_string(CONTENTION_TASKS_RAW) + " tasks, "
            + std::to_string(CONTENTION_PRODUCERS) + " producers)",
        headers, data, 70);
}
int main() {
	taskThroughput();
	futureLatency();
	mixedWorkload();
	contention();

	return 0;
}


