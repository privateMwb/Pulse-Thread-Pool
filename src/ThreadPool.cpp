#include "ThreadPool.h"

#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <utility>
#include <stdexcept>

// Lifecycle
ThreadPool::ThreadPool(std::size_t threadCount) :
	stopFlag(false),
	paused(false),
	shutdownMode(ShutdownMode::FinishTasks),
	activeTasks(0),
	exceptionCounter(0) {
	for(std::size_t i = 0; i < threadCount; ++i) {
		workers.emplace_back([this] {
			while(true) {
				std::function<void()> task;

				{
					std::unique_lock<std::mutex> lock(queueMutex);

					condition.wait(lock, [this] {
						return stopFlag || (!paused && !taskQueue.empty());
					});

					if(stopFlag) {
						if(shutdownMode == ShutdownMode::DiscardTasks) {
							return;
						}

						if(shutdownMode == ShutdownMode::FinishTasks
						        && taskQueue.empty()) {
							return;
						}
					}

					if(taskQueue.empty()) {
						continue;
					}

					task = std::move(taskQueue.front());
					taskQueue.pop();
				}
				
				ActiveTaskGuard guard(activeTasks);
				
				try {
				    task();
				} catch(...) {
				    ++exceptionCounter;
				}
			}
		});
	}
}

ThreadPool::~ThreadPool() {
	shutdown(shutdownMode);
}

// Control API
void ThreadPool::pause() {
	std::unique_lock<std::mutex> lock(queueMutex);

	paused = true;
}

void ThreadPool::resume() {
	{
	    std::unique_lock<std::mutex> lock(queueMutex);
	    
	    paused = false;
	}
	
	condition.notify_all();
}

void ThreadPool::shutdown(ShutdownMode mode) {
	{
		std::unique_lock<std::mutex> lock(queueMutex);

		shutdownMode = mode;
		stopFlag = true;
	}

	condition.notify_all();

	for(std::thread& worker : workers) {
		if(worker.joinable()) {
		    worker.join();
		}
	}
}

// Statistics 
std::size_t ThreadPool::activeTaskCount() const noexcept {
    return activeTasks.load();
}

std::size_t ThreadPool::queuedTasks() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    return taskQueue.size();
}

std::size_t ThreadPool::threadCount() const noexcept {
    return workers.size();
}

std::size_t ThreadPool::exceptionCount() const noexcept {
    return exceptionCounter.load();
}

// State
bool ThreadPool::isPaused() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    return paused;
}

bool ThreadPool::isStopped() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    return stopFlag;
}



