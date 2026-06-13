# Pulse Thread Pool

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.cppreference.com/w/cpp/23)
[![Status](https://img.shields.io/badge/status-learning%20project-green)](https://github.com/privateMwb/Pulse-Thread-Pool)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A thread pool implemented from scratch in **C++23**, built to explore concurrency primitives, task lifecycle management, move-only callable support, and real performance tradeoffs against `std::async` and raw `std::thread`.

---

## Table of Contents

- [Overview](#overview)
- [Motivation](#motivation)
- [Features](#features)
- [Design Overview](#design-overview)
  - [Internal Structure](#internal-structure)
  - [Worker Loop](#worker-loop)
  - [MoveOnlyTask](#moveonlytask)
  - [ActiveTaskGuard](#activetaskguard)
  - [Pause & Resume](#pause--resume)
  - [Shutdown Modes](#shutdown-modes)
  - [Exception Handling](#exception-handling)
- [Complexity](#complexity)
- [Quick Example](#quick-example)
  - [Fire-and-Forget](#fire-and-forget-execute)
  - [Future Results](#future-results-enqueue)
  - [Argument Forwarding](#argument-forwarding)
  - [Move-Only Callables](#move-only-callables)
  - [Pause & Resume](#pause--resume-1)
  - [Shutdown Modes](#shutdown-modes-1)
  - [Exception Handling](#exception-handling-1)
  - [Pool Statistics](#pool-statistics)
- [Core API](#core-api)
  - [Constructor & Destructor](#constructor--destructor)
  - [Task Submission](#task-submission)
  - [Control](#control)
  - [Introspection](#introspection)
- [Benchmark Results](#benchmark-results)
  - [Task Throughput](#task-throughput)
  - [Future Latency](#future-latency)
  - [Mixed Workload](#mixed-workload)
  - [Contention](#contention)
  - [Summary](#summary)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Notes](#notes)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Pulse Thread Pool is a fixed-size thread pool that pre-spawns worker threads at construction and keeps them alive until shutdown. Tasks are submitted to a shared queue and dispatched to idle workers via condition variable signaling.

It supports two submission modes — `execute()` for fire-and-forget tasks and `enqueue()` for tasks that return a `std::future` — along with pause/resume control, two shutdown strategies, move-only callable support, and live introspection of pool state.

---

## Motivation

This project was built to deeply understand:

- Thread lifecycle management (`std::thread`, join, detach)
- Synchronization primitives (`std::mutex`, `std::condition_variable`)
- Atomic operations and memory ordering
- Move-only type design (`std::unique_ptr`, type-erased wrappers)
- `std::future` and `std::packaged_task` mechanics
- RAII guards for concurrent resource tracking
- Task queue design and contention under load
- Performance tradeoffs vs `std::async` and raw `std::thread`

---

## Features

- Fixed-size worker thread pool (threads pre-spawned at construction)
- `execute()` — fire-and-forget task submission (move-only callable support)
- `enqueue()` — task submission returning `std::future<T>` with full argument forwarding
- `pause()` / `resume()` — suspend and resume task dispatching without stopping threads
- `shutdown(mode)` — two modes: drain the queue or discard pending tasks
- Move-only callable support via a custom type-erased `MoveOnlyTask` wrapper
- RAII-based active task tracking via `ActiveTaskGuard`
- Exception isolation: `execute()` exceptions are caught and counted; `enqueue()` exceptions are forwarded through the future
- Live introspection: `activeTaskCount()`, `queuedTasks()`, `threadCount()`, `exceptionCount()`
- State inspection: `isPaused()`, `isStopped()`
- Non-copyable, non-movable (safe shared ownership via pointer/reference)

---

## Design Overview

### Internal Structure

```
ThreadPool
├── workers[]          — fixed vector of std::thread (pre-spawned)
├── taskQueue          — std::queue<MoveOnlyTask> (shared work queue)
├── queueMutex         — std::mutex (guards taskQueue)
├── condition          — std::condition_variable (wakes idle workers)
├── stopFlag           — std::atomic<bool> (signals shutdown)
├── paused             — std::atomic<bool> (suspends task dispatch)
├── shutdownMode       — FinishTasks | DiscardTasks
├── activeTasks        — std::atomic<size_t> (tasks currently executing)
└── exceptionCounter   — std::atomic<size_t> (execute() exceptions caught)
```

---

### Worker Loop

Each worker thread runs the same loop from construction until shutdown:

```
while (true)
    ↓
wait on condition_variable
    ↓
wake when: stopFlag || (!paused && !taskQueue.empty())
    ↓
if stopFlag:
    DiscardTasks → return immediately
    FinishTasks  → return only when queue is empty
    ↓
pop task from queue
    ↓
ActiveTaskGuard (increment activeTasks)
    ↓
execute task
    ↓
catch any exception → increment exceptionCounter
    ↓
ActiveTaskGuard destructs (decrement activeTasks)
```

Workers block on the condition variable when idle — no busy-waiting, no spinning.

---

### MoveOnlyTask

`std::function` requires copyable callables, which rules out lambdas capturing `unique_ptr` or other move-only types. `MoveOnlyTask` solves this with a minimal type-erased wrapper:

```
MoveOnlyTask
└── unique_ptr<Base>
        └── Impl<F> : Base
                └── F fn  (the actual callable, stored by move)
                └── call() { fn(); }
```

This allows the task queue to store any callable — including those capturing move-only resources — without requiring copyability.

---

### ActiveTaskGuard

`ActiveTaskGuard` is a `[[nodiscard]]` RAII guard that automatically maintains the `activeTasks` counter:

```cpp
// On construction:  activeTasks++
// On destruction:   activeTasks--
```

It is created just before a task executes and destroyed when the task returns (or throws). This ensures `activeTaskCount()` is always accurate, even if a task throws an exception.

---

### Pause & Resume

`pause()` sets an atomic flag that causes workers to block on the condition variable even when tasks are queued. Tasks continue to accumulate in the queue but nothing is dispatched.

`resume()` clears the flag and calls `condition.notify_all()`, waking all workers to resume draining the queue.

```
pause()  → paused = true   (workers stop dequeuing)
resume() → paused = false  → notify_all() (workers wake and drain)
```

---

### Shutdown Modes

```cpp
enum class ShutdownMode {
    FinishTasks,   // drain the queue before stopping workers
    DiscardTasks   // stop workers immediately; pending tasks are dropped
};
```

Both modes set `stopFlag` and call `condition.notify_all()`. The difference is in how workers respond when they wake up:

| Mode           | Worker behavior on wake               |
|----------------|---------------------------------------|
| `FinishTasks`  | Keeps running until queue is empty    |
| `DiscardTasks` | Returns immediately, drops the queue  |

`shutdown()` is idempotent — calling it twice is safe. The destructor calls `shutdown(shutdownMode)` automatically, so explicit shutdown is optional.

---

### Exception Handling

The two submission methods handle exceptions differently by design:

| Method      | Exception behavior                                              |
|-------------|----------------------------------------------------------------|
| `execute()` | Caught internally by the worker; `exceptionCounter` incremented |
| `enqueue()` | Propagated through the `std::future`; caller retrieves via `f.get()` |

This means `execute()` tasks never crash a worker thread. `enqueue()` tasks surface exceptions to the caller cleanly via the future mechanism.

---

## Complexity

| Operation          | Complexity | Notes                                          |
|--------------------|------------|------------------------------------------------|
| `execute()`        | O(1)       | Lock + queue push + notify                     |
| `enqueue()`        | O(1)       | Lock + queue push + notify; future allocated   |
| `pause()`          | O(1)       | Atomic store                                   |
| `resume()`         | O(1)       | Atomic store + notify_all                      |
| `shutdown()`       | O(n)       | Joins all worker threads                       |
| `activeTaskCount()`| O(1)       | Atomic load                                    |
| `queuedTasks()`    | O(1)       | Lock + queue size                              |
| `threadCount()`    | O(1)       | Vector size (fixed at construction)            |
| `exceptionCount()` | O(1)       | Atomic load                                    |
| Task dispatch      | O(1)       | One worker woken per task via notify_one       |

---

## Quick Example

### Fire-and-Forget (`execute`)

```cpp
#include "ThreadPool.h"
#include <atomic>
#include <iostream>

int main() {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.execute([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

    std::cout << counter.load() << "\n"; // 100
}
```

---

### Future Results (`enqueue`)

```cpp
ThreadPool pool(4);

auto f1 = pool.enqueue([] { return 42; });
auto f2 = pool.enqueue([] { return 10 * 10; });

std::cout << f1.get() << "\n"; // 42
std::cout << f2.get() << "\n"; // 100
```

---

### Argument Forwarding

```cpp
ThreadPool pool(4);

auto f = pool.enqueue([](int a, int b) { return a + b; }, 21, 12);

std::cout << f.get() << "\n"; // 33
```

---

### Move-Only Callables

```cpp
ThreadPool pool(2);

auto resource = std::make_unique<int>(99);
std::atomic<int> result{0};

pool.execute([p = std::move(resource), &result] {
    result.store(*p, std::memory_order_relaxed);
});

pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
// result == 99
```

---

### Pause & Resume

```cpp
ThreadPool pool(4);

pool.pause();

for (int i = 0; i < 10; ++i)
    pool.execute([] { /* queued, not running */ });

// tasks are sitting in the queue — counter still 0

pool.resume();
pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);
// all 10 tasks completed
```

---

### Shutdown Modes

```cpp
// FinishTasks — waits for all queued tasks to complete
pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

// DiscardTasks — drops pending tasks immediately, stops workers ASAP
pool.shutdown(ThreadPool::ShutdownMode::DiscardTasks);
```

---

### Exception Handling

```cpp
ThreadPool pool(4);

// execute() — exception caught internally, counted
pool.execute([] {
    throw std::runtime_error("something went wrong");
});

// enqueue() — exception forwarded through the future
auto f = pool.enqueue([] -> int {
    throw std::runtime_error("future error");
    return 0;
});

pool.shutdown(ThreadPool::ShutdownMode::FinishTasks);

std::cout << pool.exceptionCount() << "\n"; // 1 (from execute)

try {
    f.get();
} catch (const std::runtime_error& e) {
    std::cout << e.what() << "\n"; // "future error"
}
```

---

### Pool Statistics

```cpp
ThreadPool pool(4);

std::cout << pool.threadCount()      << "\n"; // 4
std::cout << pool.activeTaskCount()  << "\n"; // tasks currently running
std::cout << pool.queuedTasks()      << "\n"; // tasks waiting in queue
std::cout << pool.exceptionCount()   << "\n"; // execute() exceptions caught

std::cout << std::boolalpha;
std::cout << pool.isPaused()  << "\n"; // false
std::cout << pool.isStopped() << "\n"; // false
```

---

## Core API

### Constructor & Destructor

```cpp
explicit ThreadPool(std::size_t threadCount);
~ThreadPool(); // calls shutdown(shutdownMode) automatically
```

ThreadPool is non-copyable and non-movable:

```cpp
ThreadPool(const ThreadPool&)            = delete;
ThreadPool& operator=(const ThreadPool&) = delete;
ThreadPool(ThreadPool&&)                 = delete;
ThreadPool& operator=(ThreadPool&&)      = delete;
```

---

### Task Submission

```cpp
// Fire-and-forget. Accepts move-only callables.
// Throws std::runtime_error if pool is stopped.
template<typename F>
void execute(F&& task);

// Returns std::future<T>. Forwards arguments to the task.
// Throws std::runtime_error if pool is stopped.
template<typename F, typename... Args>
[[nodiscard]] auto enqueue(F&& task, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;
```

---

### Control

```cpp
void pause()  noexcept;  // suspend task dispatch (tasks still queue)
void resume() noexcept;  // resume dispatch, wake all workers

void shutdown(ShutdownMode mode = ShutdownMode::FinishTasks) noexcept;
// FinishTasks  — drain queue then stop
// DiscardTasks — stop immediately, drop pending tasks
// Safe to call multiple times (idempotent)
```

---

### Introspection

```cpp
[[nodiscard]] std::size_t activeTaskCount()  const noexcept; // tasks currently executing
[[nodiscard]] std::size_t queuedTasks()      const noexcept; // tasks waiting in queue
[[nodiscard]] std::size_t threadCount()      const noexcept; // fixed worker count
[[nodiscard]] std::size_t exceptionCount()   const noexcept; // execute() exceptions caught

[[nodiscard]] bool isPaused()  const noexcept;
[[nodiscard]] bool isStopped() const noexcept;
```

---

## Benchmark Results

Benchmarks compare Pulse Thread Pool against `std::async` and raw `std::thread`. All times in microseconds (µs).

> **Note:** Task counts differ between ThreadPool and the alternatives — spawning 100,000 raw threads is not feasible, so the pool handles significantly more tasks than `std::async`/`std::thread` in the same benchmark. This reflects the fundamental advantage of thread reuse. Compiled without `-O2`.

---

### Task Throughput

100,000 tasks (ThreadPool) vs 1,000 tasks (`std::async` / `std::thread`)

| Method       | Time (µs)  | Notes                             |
|--------------|----------:|-----------------------------------|
| ThreadPool   | 308,151   | 100× more tasks                   |
| std::async   | 173,577   | 1,000 tasks                       |
| std::thread  | 199,691   | 1,000 tasks                       |

ThreadPool processes **100× more tasks** in ~1.5× the time of `std::async` — meaning per-task cost is roughly **66× lower**.

---

### Future Latency

1,000 sequential `enqueue()` + `future.get()` round-trips

| Method       | Time (µs)  | vs ThreadPool  |
|--------------|----------:|----------------|
| ThreadPool   | 117,785   | baseline        |
| std::async   | 383,744   | **3.3× slower** |
| std::thread  | 428,934   | **3.6× slower** |

ThreadPool's pre-spawned workers eliminate thread creation overhead entirely. `std::async` and `std::thread` pay that cost on every round-trip.

---

### Mixed Workload

Short (10 iters), medium (1,000 iters), and long (10,000 iters) tasks — 10,000 tasks (ThreadPool) vs 1,000 (`std::async` / `std::thread`)

| Method       | Time (µs)  | Notes                              |
|--------------|----------:|------------------------------------|
| ThreadPool   | 29,597    | 10,000 tasks                       |
| std::async   | 203,886   | 1,000 tasks — **6.9× slower**      |
| std::thread  | 201,187   | 1,000 tasks — **6.8× slower**      |

ThreadPool handles **10× more tasks** in ~15% of the time. Per-task, it's roughly **69× more efficient** on mixed workloads.

---

### Contention

4 producers hammering the queue simultaneously — 100,000 tasks (ThreadPool) vs 4,000 (`std::async` / `std::thread`)

| Method       | Time (µs)   | Notes                               |
|--------------|------------:|-------------------------------------|
| ThreadPool   | 201,776     | 100,000 tasks, 4 producers          |
| std::async   | 673,805     | 4,000 tasks — **3.3× slower**       |
| std::thread  | 2,601,702   | 4,000 tasks — **12.9× slower**      |

This is the most telling benchmark. Raw `std::thread` collapses under multi-producer contention — OS thread creation at scale is extremely expensive. ThreadPool's single shared queue with pre-spawned workers absorbs the pressure cleanly.

---

### Summary

| Benchmark        | ThreadPool tasks | Competitor tasks | Per-task advantage |
|------------------|:----------------:|:----------------:|:------------------:|
| Task Throughput  | 100,000          | 1,000            | ~66× per task      |
| Future Latency   | 1,000            | 1,000            | ~3.3× per task     |
| Mixed Workload   | 10,000           | 1,000            | ~69× per task      |
| Contention       | 100,000          | 4,000            | ~65× per task      |

> The thread pool model wins by eliminating thread creation and destruction on every task. The bigger the workload, the more this compounds — especially under contention where raw thread spawning serializes and degrades catastrophically.

---

## Project Structure

```
Pulse-Thread-Pool/
├── include/
│   ├── ThreadPool.h       # Class declaration, MoveOnlyTask, ActiveTaskGuard
│   └── ThreadPool.tpp     # enqueue() and execute() template definitions
│
├── src/
│   └── ThreadPool.cpp     # Constructor, destructor, worker loop, control methods
│
├── benchmarks/
│   ├── benchmarks.cpp     # Throughput, latency, mixed, contention benchmarks
│   └── utils/
│       ├── Table.h        # Benchmark result table formatting
│       └── Table.tpp
│
├── tests/
│   └── test.cpp           # 13 unit tests covering correctness and concurrency
│
├── examples/
│   └── examples.cpp       # Usage examples for all major features
│
├── README.md
└── LICENSE
```

---

## Build Instructions

### Requirements

- C++23-compatible compiler: GCC 13+, Clang 17+, or MSVC 19.38+
- No external dependencies

### Compile & Run Tests

```bash
g++ -std=c++23 tests/test.cpp src/ThreadPool.cpp -Iinclude -o build/tests
./build/tests
```

### Compile & Run Examples

```bash
g++ -std=c++23 examples/examples.cpp src/ThreadPool.cpp -Iinclude -o build/examples
./build/examples
```

### Compile & Run Benchmarks

```bash
g++ -std=c++23 benchmarks/benchmarks.cpp src/ThreadPool.cpp -Iinclude -Ibenchmarks/utils -o build/benchmarks
./build/benchmarks
```

> Use `-O2` or `-O3` for production-representative benchmark results. On Linux, link with `-lpthread` if needed.

---

## Notes

- **Not production-ready.** This is an educational project — consider libraries like `BS::thread_pool` or Intel TBB for real workloads.
- The task queue uses a single `std::mutex`. Under extreme producer contention, this can become a bottleneck — a lock-free queue would reduce this.
- `pause()` does not drain in-flight tasks — it only stops new tasks from being dequeued. Tasks already executing on workers continue to run.
- `execute()` exceptions are silently counted. If you need to know *which* task threw, use `enqueue()` instead and inspect the future.
- Iterator invalidation does not apply, but **iterator analogue**: any `std::future` returned by `enqueue()` becomes inaccessible if you don't store it — the `[[nodiscard]]` attribute enforces this at compile time.
- Thread count is fixed at construction. Dynamic resizing is not supported.

---

## Contributing

Learning-focused PRs and improvements are welcome. Some areas worth exploring:

- Lock-free task queue (reduce mutex contention under high producer load)
- Dynamic thread count adjustment (`resize()`)
- Task priorities (priority queue backend)
- `wait()` method — block until the queue is empty without shutting down
- CMake build system
- CI pipeline (GitHub Actions)

---

## License

[MIT](LICENSE) — free to use, modify, and distribute for educational and personal purposes.
