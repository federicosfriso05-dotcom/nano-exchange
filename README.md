# NanoExchange
A low-latency matching engine written in C++17, focused on performance and memory efficiency. Uses a custom O(1) memory pool to avoid heap allocations at runtime. The goal is to minimize allocations and improve throughput under heavy load.

## Key features:
* **Zero-allocation runtime**: memory pool completely bypasses OS heap (new/delete) during execution
* **Cache-locality optimized**: data structures designed to minimize CPU cache misses during high-throughput matching
* **Lock-free multithreaded pipeline**: three-stage producer-consumer architecture (gateway → matching → reporter) built on a custom SPSC queue with `alignas(64)` cache-line isolation, eliminating mutex contention from the critical path
* **Zero-cost trade callback**: `OrderBook` is templated on its fill-reporting callback, replacing `std::function` virtual dispatch and heap allocation with a fully inlined, compiler-optimized call — or a no-op eliminated entirely at compile time when no callback is provided
* **Micro-benchmarked**: nanosecond-level latency tracking powered by Google Benchmark

## Performance Metrics

### Hardware Architecture
All benchmarks were executed locally on the following architecture:
* **CPU:** Apple Silicon M1 series
* **RAM:** 8 GB
* **OS:** macOS
* **Compiler:** Apple Clang (C++17, `-O3` Release Mode, `-march=native`)

### Naive vs. Optimized Speedup
To prove the efficacy of the data structures, the engine was benchmarked against a "Naive" implementation (using standard `std::map` and dynamic heap allocations). 

* **Naive Implementation:** 161 ns/op
* **NanoExchange (Optimized):** `21.00 ns/op`
* **Net Speedup:** `~7.7x faster`

### Matching Core — Isolated Latency (lat_OptimizedOrderBook)
<details>
<summary><b>Click to expand raw terminal output</b></summary>

```console
$ ./build/lat_OptimizedOrderBook
Samples: 2000 batches x 1000 orders  (batch avg, steady_clock)
p50:   20.92 ns
p90:   22.58 ns
p99:   26.46 ns
p99.9: 30.21 ns
```
</details>

### End-to-End Pipeline 3-Thread System (nano_exchange_app)
<details>
<summary><b>Click to expand raw terminal output</b></summary>

```console
$ ./build/nano_exchange_app
System throughput and latency report:
  Total Orders:      1000000
  Distribution:      500715 Buys / 499285 Sells
  Total Volume:      50484550 shares

Performance:
  Wall Time:         0.0922 seconds
  Avg Throughput:    10849699 ops/sec

  LATENCY DISTRIBUTION (Per Order)
  p50 (Median):      73.79 ns
  p90:               120.38 ns
  p99 (Tail):        141.50 ns
  p99.9 (Extreme):   156.83 ns
  Max:               156.83 ns
```
</details>

### Note on Apple Silicon Benchmarks: 
The engine's core execution time (~21 ns) is strictly faster than the macOS ARM hardware timer resolution, which is physically locked at 24 MHz (~41.67 ns tick rate). To bypass this hardware quantization limit, `lat_OptimizedOrderBook` uses batch averaging: 1000 orders are timed per sample and the result is divided, yielding ~0.04 ns effective resolution. The pipeline throughput (~11M ops/sec) is measured end-to-end by `nano_exchange_app`.

## Design Decisions

This project focuses on building a low-latency matching engine with predictable performance.  
Below are the key design choices and the trade-offs behind them.

### 1. Contiguous Price Levels (Vector vs Map)

Instead of using associative containers like `std::map`, the order book is implemented as two contiguous arrays (`std::vector<PriceLevel>`), indexed directly by price.

**Why:**
- O(1) direct access to price levels
- Better cache locality compared to tree-based structures
- Avoids pointer chasing and branch-heavy traversals

**Trade-off:**
- Requires a bounded price range (`max_price`)
- Higher memory usage if the price space is sparse


### 2. Custom Memory Pool (Avoiding Heap Allocations)

Orders are allocated using a custom memory pool instead of `new/delete`.

**Why:**
- Eliminates runtime heap allocations
- Reduces allocator overhead and fragmentation
- Ensures predictable latency during matching

**Trade-off:**
- Fixed maximum number of orders
- Manual memory management increases implementation complexity


### 3. Intrusive Linked Lists (For Price Level)

Each `PriceLevel` maintains orders using an intrusive doubly linked list.

**Why:**
- O(1) insertion and removal
- No extra allocations for list nodes
- Naturally preserves FIFO (price-time priority)

**Trade-off:**
- Orders must carry `next/prev` pointers
- Less flexibility compared to standard containers


### 4. Bitset-Based Price Discovery

A custom bitset is used to track which price levels are active.

**Why:**
- Quickly find next best bid/ask using bit operations (`ctz`, `clz`)
- Avoids linear scans across empty price levels
- Reduces worst-case matching latency

**Trade-off:**
- Slightly more complex implementation
- Depends on compiler intrinsics for optimal performance


### 5. Pre-Allocated Order Index (Vector vs Hash Map)

Order lookup is implemented using a pre-allocated vector (`orderVector`) indexed by order ID.

**Why:**
- O(1) lookup without hashing
- Better cache locality compared to `std::unordered_map`
- Lower and more predictable latency

**Trade-off:**
- Requires bounded order ID space
- Potential memory overhead if IDs are sparse


### 6. Separation of Aggressive vs Passive Phases

Order processing is split into:
- **Aggressive phase:** matching against the book
- **Passive phase:** insertion of remaining quantity

**Why:**
- Clear separation of logic improves readability and correctness
- Matches how real matching engines are conceptually structured

### 7. Lock-Free SPSC Queue (Pipeline Architecture)

The matching engine is surrounded by a three-stage pipeline: gateway → matching → reporter,
connected by two lock-free Single Producer Single Consumer queues.

**Why:**
- The matching core never blocks on I/O, logging, or reporting
- No mutex means no kernel involvement on the critical path
- `alignas(64)` on `write_idx` and `read_idx` prevents false sharing between producer
  and consumer cache lines
- `memory_order_release/acquire` pairs provide the minimum synchronization needed —
  no heavier `seq_cst` overhead

**Trade-off:**
- Latency per order increases vs. single-thread (~75 ns vs ~23 ns) because crossing
  the queue costs ~50 ns — multithreading buys throughput and isolation, not raw latency
- SPSC constraint: exactly one producer and one consumer per queue; adding more
  producers would require a heavier MPSC design


### Summary

The overall design prioritizes:
- Low and predictable latency
- Cache efficiency
- O(1) operations where possible

At the cost of:
- Increased implementation complexity
- Multithreading adds pipeline latency (~50 ns per queue crossing) vs. single-thread
- Reduced flexibility compared to general-purpose data structures

## Quick Start

**Prerequisites:**
* CMake 3.24+
* C++17 compiler (GCC, Clang, or Apple Clang)

**Building the Project:**
For maximum performance, the engine must be built in `Release` mode to enable `-O3` compiler optimizations.

```bash
# 1. Configure the build system
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 2. Compile the engine, tests, and benchmarks
cmake --build build --parallel
```

## Usage

Once compiled, all executables are located inside the `build` directory. 

**Run the Main Application:**
```bash
./build/nano_exchange_app
```
**Run the unit tests:**
```bash
./build/nano_exchange_tests
```
**Run the standalone latency microbenchmark (p50/p90/p99):**
```bash
./build/lat_OptimizedOrderBook
```
**Run the Google Benchmark suite:**
```bash
./build/bm_OptimizedOrderBook
```

## Project Structure

```text
nano-exchange/
├── benchmarks/              # Google Benchmark latency tracking
├── include/nano_exchange/   # Core library headers (MemoryPool, OrderBook, ...)
├── src/                     # Main engine implementation and entry point
├── tests/                   # GoogleTest unit test suite
├── .clang-format            # Strict C++ styling rules
└── CMakeLists.txt           # Modern target-based build configuration
```

## License

This project is open-source and developed for educational purposes and performance demonstration.



