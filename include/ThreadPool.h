#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <future>
#include <type_traits>
#include <stdexcept>
#include <memory>
#include <utility>
#include <atomic>

class ThreadPool {
public:
    // Types 
	enum class ShutdownMode {
		FinishTasks,
		DiscardTasks
	};

private:
    // Helpers
	struct ActiveTaskGuard {
	private:
		std::atomic<std::size_t>& counter;

	public:
		ActiveTaskGuard(std::atomic<std::size_t>& c) :
			counter(c) {
			++counter;
		}

		~ActiveTaskGuard() {
			--counter;
		}
		
		ActiveTaskGuard(const ActiveTaskGuard&) = delete;
		ActiveTaskGuard& operator=(const ActiveTaskGuard&) = delete;
		
		ActiveTaskGuard(ActiveTaskGuard&&) = delete;
		ActiveTaskGuard& operator=(ActiveTaskGuard&&) = delete;
	};
	
	// Workers Thread
	std::vector<std::thread> workers;

	// Task Queue
	std::queue<std::function<void()>> taskQueue;

	// Synchronization
	mutable std::mutex queueMutex;
	std::condition_variable condition;

	// Control Flags
	bool stopFlag;
	bool paused;
	ShutdownMode shutdownMode;

	// Statistics
	std::atomic<std::size_t> activeTasks;
	std::atomic<std::size_t> exceptionCounter;

public:
	// Lifecycle
	explicit ThreadPool(std::size_t threadCount);
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	// Control API
	void pause();
	void resume();
	void shutdown(ShutdownMode mode);

	// Task Submission
	template<typename F, typename... Args>
	auto enqueue(F&& task, Args&&... args)
	-> std::future<std::invoke_result_t<std::decay_t<F>, Args...>>;

	// Task Execution
	template<typename F>
	void execute(F&& task);

	// Statistics
	std::size_t activeTaskCount() const noexcept;
	std::size_t queuedTasks() const noexcept;
	std::size_t threadCount() const noexcept;
	std::size_t exceptionCount() const noexcept;

	// State
	bool isPaused() const noexcept;
	bool isStopped() const noexcept;
};

#include "ThreadPool.tpp"
