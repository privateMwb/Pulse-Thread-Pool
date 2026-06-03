#include <iostream>
#include <mutex>
#include <iomanip>
#include <string>
#include <thread>
#include <sstream>
#include <chrono>
#include <atomic>

#include "ThreadPool.h"

std::string center(const std::string& text, std::size_t width) {
	if(text.size() >= width) {
		return text;
	}

	std::size_t left = (width - text.size()) / 2;
	std::size_t right = width - text.size() - left;

	return std::string(left, ' ')
	       + text
	       + std::string(right, ' ');
}

template<typename T>
std::string format(const T& value, const std::string& suffix = "") {
	std::ostringstream oss;
	oss << value;

	return oss.str() + suffix;
}

void basic_execution() {
	ThreadPool pool(4);

	std::mutex coutMutex;

	std::string title = "Basic Execution (" + format(pool.threadCount()) + " threads)";

	std::cout << std::left
	          << center(title, 25)
	          << "\n\n";
	std::cout << std::left
	          << center("Task", 10)
	          << center("Thread ID", 15)
	          << "\n";

	for(std::size_t i = 0; i < 10; ++i) {
		pool.execute([i, &coutMutex] {
			std::unique_lock<std::mutex> lock(coutMutex);

			std::cout << std::left
			          << center(format(i), 10)
			          << center(format(std::this_thread::get_id()), 15)
			          << "\n";

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		});
	}

	pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

	std::cout << "\n";
}

void future_results() {
	ThreadPool pool(4);

	std::cout << center("Future Results", 20)
	          << "\n\n";

	std::cout << std::left
	          << center("I", 10)
	          << center("I x I", 10)
	          << "\n";
	for(std::size_t i = 1; i <= 10; ++i) {
		auto future = pool.enqueue([i] {
			return i * i;
		});

		std::cout << std::left
		          << center(format(i), 10)
		          << center(format(future.get()), 10)
		          << "\n";
	}
	
	pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
	
	std::cout << "\n";
}

void resume_pause() {
	ThreadPool pool(4);

	constexpr std::size_t count = 1'000'000;
	std::atomic<int> counter{0};

	std::string title = "Pause & Resume (" + format(count) + " counts)";
	std::cout << center(title, 25)
	          << "\n\n";

	auto display = [](bool paused, const std::atomic<int>& c) {
		std::cout << std::left
		          << center((paused ? "Paused" : "Resume"), 10)
		          << center(format(c.load()), 15)
		          << "\n";
	};

	pool.pause();

	for(std::size_t i = 0; i < count; ++i) {
		pool.execute([&counter] {
			++counter;
		});
	}

	std::cout << std::left
	          << center("State", 10)
	          << center("Counter", 15)
	          << "\n";

	display(pool.isPaused(), counter);

	while(counter.load() < count - 100'000) {
		pool.resume();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		pool.pause();
		display(pool.isPaused(), counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	pool.resume();
	pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
	display(pool.isPaused(), counter);

	std::cout << "\n";
}

void shutdown_modes() {
	constexpr std::size_t count = 1'000'000;

	std::string title = "Shutdown Modes (" + format(count) + " counts)";

	std::cout << center(title, 45)
	          << "\n\n";
	std::cout << std::left
	          << center("Pool", 10)
	          << center("Mode", 15)
	          << center("Counter == Count", 20)
	          << "\n";

	ThreadPool pool1(4);
	ThreadPool pool2(4);

	std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};
    
	for(std::size_t i = 0; i < count; ++i) {
		pool1.execute([&counter1] {
			++counter1;
		});
	}
	
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	
	pool1.shutdown(ThreadPool::ShutdownMode::FinishTasks);
	
	for(std::size_t i = 0; i < count; ++i) {
		pool2.execute([&counter2] {
		    std::this_thread::sleep_for(std::chrono::milliseconds(10));
			++counter2;
		});
	}
	
	pool2.shutdown(ThreadPool::ShutdownMode::DiscardTasks);
	
	std::string c1 = format(counter1.load()) + " / " + format(count);
	std::string c2 = format(counter2.load()) + " / " + format(count);
	
	std::cout << std::left
	          << center("1", 10)
	          << center("Finish Tasks", 15)
	          << center(c1, 20)
	          << "\n";
	          
    std::cout << std::left
              << center("2", 10)
	          << center("Discard Tasks", 15)
	          << center(c2, 20)
	          << "\n";
	          
	std::cout << "\n";
}

void pool_statistics() {
    ThreadPool pool(4);
    
    std::cout << center("Pool Statistics", 25)
              << "\n\n";
    
    for(std::size_t i = 0; i < 100; ++i) {
        pool.execute([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "Thread Count: " << pool.threadCount() << "\n";
    std::cout << "Active Tasks Count: " << pool.activeTaskCount() << "\n";
    std::cout << "Queued Tasks: " << pool.queuedTasks() << "\n";
    
    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
}

int main() {
	basic_execution();

	future_results();

	resume_pause();

	shutdown_modes();
	
	pool_statistics();
	
	return 0;
}