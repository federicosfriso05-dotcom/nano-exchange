#include <benchmark/benchmark.h>
#include <cstdint>
#include "nano_exchange/MemoryPool.hpp"

struct DummyOrder 
{
    uint64_t id;
    uint64_t price;
};

// First try with dynamic allocation
static void BM_StandardNewDelete(benchmark::State& state) 
{
    for (auto _ : state) 
    {
        DummyOrder* order = new DummyOrder();
        benchmark::DoNotOptimize(order); 
        // Giving it back to the OS
        delete order;
    }
}
BENCHMARK(BM_StandardNewDelete);

// Second try with the O(1) intrusive free list
static void BM_MemoryPoolAllocDealloc(benchmark::State& state) 
{
    nano_exchange::MemoryPool<DummyOrder> pool(1000000); 

    for (auto _ : state) 
    {
        DummyOrder* order = pool.create();
        benchmark::DoNotOptimize(order);
        pool.destroy(order);
    }
}
BENCHMARK(BM_MemoryPoolAllocDealloc);

BENCHMARK_MAIN();
