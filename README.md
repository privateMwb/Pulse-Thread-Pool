# PulseThreadPool

PulseThreadPool is a modern C++23 thread pool library built for learning and exploring concurrency and multithreaded system design.

It was built to explore:
- thread management
- task scheduling and execution
- synchronization primitives (mutex, condition variable)
- futures and promises
- RAII in concurrent systems
- atomic operations
- performance benchmarking of concurrent workloads

This project is primarily educational and is not intended to replace production-grade libraries such as Thread Building Blocks or Boost.Asio.
The goal is to understand how thread pools and task scheduling work internally.

---

# Features

## Task Execution

Supports immediate execution of tasks using worker threads.

- Efficient task queue
- Lock-based synchronization
- Automatic worker distribution
- Exception-safe execution

---

## Task Submission (enqueue)

Supports submitting tasks with return values using futures.

- `std::future` support
- Perfect forwarding
- Variadic arguments
- Packaged task wrapping

---

## Pause & Resume

Allows temporarily stopping task execution without destroying the pool.

- Pause execution safely
- Resume processing queued tasks
- Thread-safe state transitions

---

## Shutdown Modes

### FinishTasks
- Completes all queued tasks before stopping

### DiscardTasks
- Stops immediately and discards remaining tasks

---

## Statistics

- active task count
- queued task count
- exception count
- worker thread count

---

# Project Structure

```txt
PulseThreadPool/
├── include/
│   ├── ThreadPool.h
│   └── ThreadPool.tpp
│
├── src/
│   └── ThreadPool.cpp
│
├── examples/
│   └── examples.cpp
│
├── tests/
│   └── test.cpp
│
├── benchmarks/
│   └── benchmark.cpp
│
└── README.md

---

# Build

## Example

```bash
g++ -std=c++23 examples/example.cpp -Iinclude -o example
./example
```

## Benchmark

```bash
g++ -std=c++23 benchmarks/benchmark.cpp -Iinclude -o benchmark
./benchmark
```

## Test

```bash
g++ -std=c++23 tests/test.cpp -Iinclude -o test
./test
```

---

# Notes

This project is designed for educational purposes and focuses on clarity over extreme optimization.

It demonstrates modern C++23 features including:

- templates and type deduction
- concurrency primitives
- RAII-based resource management
- atomic operations
- futures and asynchronous task execution

---

# License

MIT License