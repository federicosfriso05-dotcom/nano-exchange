#include <benchmark/benchmark.h>
#include <cstdint>
#include "nano_exchange/memory_pool.hpp"

// The same dummy order we used in testing
struct DummyOrder {
    uint64_t id;
    uint64_t price;
};

// --- COMPETITOR 1: The Standard OS Allocator (Slow) ---
static void BM_StandardNewDelete(benchmark::State& state) {
    for (auto _ : state) {
        // Asking the OS for memory
        DummyOrder* order = new DummyOrder();
        benchmark::DoNotOptimize(order); 
        // Giving it back to the OS
        delete order;
    }
}
BENCHMARK(BM_StandardNewDelete);

// --- COMPETITOR 2: Our Custom HFT Memory Pool (Fast) ---
static void BM_MemoryPoolAllocDealloc(benchmark::State& state) {
    // Pre-allocate the massive block ONCE, outside the timed loop
    nano_exchange::MemoryPool<DummyOrder> pool(1000000); 

    for (auto _ : state) {
        // O(1) Intrusive Free List allocation
        DummyOrder* order = pool.allocator();
        benchmark::DoNotOptimize(order);
        // O(1) return to the Free List
        pool.deallocator(order);
    }
}
BENCHMARK(BM_MemoryPoolAllocDealloc);

// This macro generates the main() function for the benchmark executable
BENCHMARK_MAIN();
