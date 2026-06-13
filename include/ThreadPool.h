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
	// Shutdown Mode
	enum class ShutdownMode {
		FinishTasks,
		DiscardTasks
	};

private:
	// Move-Only Task Wrapper
	struct MoveOnlyTask {
		struct Base {
			virtual void call() = 0;
			virtual ~Base()    = default;
		};

		template<typename F>
		struct Impl : Base {
			F fn;
			explicit Impl(F&& f) : fn(std::move(f)) {}
			void call() override { fn(); }
		};

		std::unique_ptr<Base> ptr;

		MoveOnlyTask() : ptr(nullptr) {}

		template<typename F>
		MoveOnlyTask(F&& f)
			: ptr(std::make_unique<Impl<std::decay_t<F>>>(std::forward<F>(f))) {}

		MoveOnlyTask(MoveOnlyTask&&)            = default;
		MoveOnlyTask& operator=(MoveOnlyTask&&) = default;

		MoveOnlyTask(const MoveOnlyTask&)            = delete;
		MoveOnlyTask& operator=(const MoveOnlyTask&) = delete;

		explicit operator bool() const { return ptr != nullptr; }
		void operator()() { if (ptr) ptr->call(); }
	};

	// Active Task Guard
	struct [[nodiscard]] ActiveTaskGuard {
	private:
		std::atomic<std::size_t>& counter;

	public:
		explicit ActiveTaskGuard(std::atomic<std::size_t>& c) : counter(c) {
			counter.fetch_add(1, std::memory_order_relaxed);
		}

		~ActiveTaskGuard() {
			counter.fetch_sub(1, std::memory_order_relaxed);
		}

		ActiveTaskGuard(const ActiveTaskGuard&)            = delete;
		ActiveTaskGuard& operator=(const ActiveTaskGuard&) = delete;

		ActiveTaskGuard(ActiveTaskGuard&&)                 = delete;
		ActiveTaskGuard& operator=(ActiveTaskGuard&&)      = delete;
	};

	// Worker Threads
	std::vector<std::thread> workers;

	// Task Queue
	std::queue<MoveOnlyTask> taskQueue;

	// Synchronization
	mutable std::mutex      queueMutex;
	std::condition_variable condition;

	// Control Flags
	std::atomic<bool> stopFlag;
	std::atomic<bool> paused;
	ShutdownMode      shutdownMode;

	// Statistics
	std::atomic<std::size_t> activeTasks;
	std::atomic<std::size_t> exceptionCounter;

public:
	// Constructors & Destructor
	explicit ThreadPool(std::size_t threadCount);
	~ThreadPool();

	ThreadPool(const ThreadPool&)            = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	ThreadPool(ThreadPool&&)                 = delete;
	ThreadPool& operator=(ThreadPool&&)      = delete;

	// Control
	void pause()  noexcept;
	void resume() noexcept;
	void shutdown(ShutdownMode mode = ShutdownMode::FinishTasks) noexcept;

	// Task Submission
	template<typename F, typename... Args>
	[[nodiscard]] auto enqueue(F&& task, Args&&... args)
	-> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

	template<typename F>
	void execute(F&& task);

	// Introspection
	[[nodiscard]] std::size_t activeTaskCount()  const noexcept;
	[[nodiscard]] std::size_t queuedTasks()      const noexcept;
	[[nodiscard]] std::size_t threadCount()      const noexcept;
	[[nodiscard]] std::size_t exceptionCount()   const noexcept;

	[[nodiscard]] bool isPaused()  const noexcept;
	[[nodiscard]] bool isStopped() const noexcept;
};

#include "ThreadPool.tpp"
