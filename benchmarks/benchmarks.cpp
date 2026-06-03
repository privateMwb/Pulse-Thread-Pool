#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>

#include "ThreadPool.h"

auto clock_now() {
	return std::chrono::high_resolution_clock::now();
}

auto execute_throughput(std::size_t TASKS) {
	ThreadPool pool(4);

	std::atomic<int> counter{0};

	auto start = clock_now();

	for(std::size_t i = 0; i < TASKS; ++i) {
		pool.execute([&counter] {
			++counter;
		});
	}

	pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

	auto end = clock_now();

	return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

auto enqueue_throughput(std::size_t TASKS) {
	ThreadPool pool(4);

	auto start = clock_now();

	for(std::size_t i = 0; i < TASKS; ++i) {
		pool.enqueue([i] {
			return i;
		});
	}

	pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

	auto end = clock_now();

	return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

void benchmark_throughput() {
	constexpr std::size_t TASKS = 100'000;

	auto execute_duration = execute_throughput(TASKS);
	auto enqueue_duration = enqueue_throughput(TASKS);

	std::cout << "Benchmark Throughput " << "(" << TASKS << " tasks)\n\n";
    
    std::cout << std::left
              << std::setw(12) << "Function "
              << std::setw(10) << "Time" 
              << std::setw(10) << "Throughput"
              << "\n";
              
	std::cout << std::left 
	          << std::setw(12) << "[execute]"
	          << std::setw(10) << (std::to_string(execute_duration) + " ms")
	          << std::setw(10) << (std::to_string((TASKS * 1000) / execute_duration) + " tasks/sec")
	          << "\n";

    std::cout << std::left 
	          << std::setw(12) << "[enqueue]"
	          << std::setw(10) << (std::to_string(enqueue_duration) + " ms")
	          << std::setw(10) << (std::to_string((TASKS * 1000) / enqueue_duration) + " tasks/sec")
	          << "\n\n";
}

void execute_scaling(std::size_t TASKS, std::vector<long long>& durations) {
	for(int thread = 1; thread <= 8; thread *= 2) {
		ThreadPool pool(thread);

		std::atomic<int> counter{0};

		auto start = clock_now();

		for(std::size_t i = 0; i < TASKS; ++i) {
			pool.execute([&counter] {
				++counter;
			});
		}

		pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

		auto end = clock_now();

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		durations.push_back(duration);
	}
}

void enqueue_scaling(std::size_t TASKS, std::vector<long long>& durations) {
	for(int thread = 1; thread <= 8; thread *= 2) {
		ThreadPool pool(thread);

		auto start = clock_now();

		for(std::size_t i = 0; i < TASKS; ++i) {
			pool.enqueue([i] {
				return i;
			});
		}

		pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

		auto end = clock_now();

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		durations.push_back(duration);
	}
}


void benchmark_scaling() {
	constexpr std::size_t TASKS = 100'000;
	
	std::vector<long long> execute_durations;
	std::vector<long long> enqueue_durations;
	std::vector<int> threads{1, 2, 4, 8};
	
	execute_scaling(TASKS, execute_durations);
	enqueue_scaling(TASKS, enqueue_durations);
	
	std::cout << "Benchmark Scaling " << "(" << TASKS << " tasks)\n\n";
	std::cout << std::left 
	          << std::setw(10) << "Threads"
	          << std::setw(18) << "[execute] Time"
	          << std::setw(18) << "[enqueue] Time" 
	          << "\n";
	          
	for(std::size_t i = 0; i<threads.size(); ++i) {
	    std::cout << std::left 
	              << std::setw(10) << threads[i]
	              << std::setw(18) << std::to_string(execute_durations[i]) + " ms" 
	              << std::setw(18) << std::to_string(enqueue_durations[i]) + " ms"
	              << "\n";
	}
}

int main() {
	benchmark_throughput();

	benchmark_scaling();

	return 0;
}

