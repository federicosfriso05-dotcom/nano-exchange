#include <benchmark/benchmark.h>
#include "nano_exchange/OrderBook.hpp" 
#include <vector>
#include <random>

using namespace nano_exchange; 

// Pre-generating orders isolates the engine's execution time from 
// std::mt19937 random number generation latency (~25ns per order).
struct PreRolledOrder
{
    bool is_buy;
    uint64_t price;
    uint32_t qty;
};

static void BM_OptimizedOrderBook_Processing(benchmark::State& state)
{
    uint64_t max_price = 100000;
    size_t max_orders = 10000000; 
    
    MemoryPool<Order> pool(max_orders);
    OrderBook book(max_price, max_orders, pool);

    // 8192 elements * 16 Bytes =~ 131KB. It fits perfectly in the 
    // CPU L1-L2 cache, avoiding RAM fetch latency
    const size_t CACHE_SIZE = 8192; 
    std::vector<PreRolledOrder> pre_rolled(CACHE_SIZE);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> price_dist(1000, 2000);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> type_dist(0, 1); 

    for (size_t i = 0; i < CACHE_SIZE; ++i)
    {
        pre_rolled[i].is_buy = (type_dist(rng) == 0);
        pre_rolled[i].price = price_dist(rng);
        pre_rolled[i].qty = qty_dist(rng);
    }

    uint32_t order_id = 1;
    size_t idx = 0;

    for (auto _ : state)
    {
        // Instant L1 Cache read
        const auto& data = pre_rolled[idx];

        Order* order = pool.create();
        *order = Order{order_id, data.price, data.qty, data.is_buy ? Side::Buy : Side::Sell};

        book.submit_order(order);

        // Prevents CPU pipeline flushes caused by failed 'if' branch predictions.
        idx = (idx + 1) & 8191; 
        
        order_id++;
        if (order_id >= max_orders) order_id = 1; 
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Push it all the way to 10 Million iterations
BENCHMARK(BM_OptimizedOrderBook_Processing)->Iterations(10000000);

BENCHMARK_MAIN();