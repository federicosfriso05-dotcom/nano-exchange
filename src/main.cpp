#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <numeric>

#include "nano_exchange/Order.hpp"
#include "nano_exchange/MemoryPool.hpp"
#include "nano_exchange/OrderBook.hpp"

using namespace nano_exchange;

int main() 
{
    // Compile-time constants
    constexpr uint64_t MAX_PRICE = 100000;   
    constexpr size_t NUM_ORDERS = 1000000;   
    
    // Batching prevents std::chrono quantization errors on sub-100ns measurements
    constexpr size_t BATCH_SIZE = 1000;      
    constexpr size_t NUM_BATCHES = NUM_ORDERS / BATCH_SIZE;

    MemoryPool<Order> pool(NUM_ORDERS + 1);
    OrderBook book(MAX_PRICE, NUM_ORDERS + 1, pool);

    std::vector<Order*> orders_to_submit;
    orders_to_submit.reserve(NUM_ORDERS);

    std::mt19937 rng(42); 
    std::uniform_int_distribution<uint64_t> price_dist(49800, 50200); 
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    size_t total_buys = 0;
    size_t total_sells = 0;
    uint64_t total_volume = 0;

    for (size_t i = 1; i <= NUM_ORDERS; ++i) 
    {
        Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
        uint64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);
        
        if (side == Side::Buy) total_buys++; else total_sells++;
        total_volume += qty;

        orders_to_submit.push_back(pool.create(i, price, qty, side));
    }
    
    std::vector<double> batch_latencies_ns;
    batch_latencies_ns.reserve(NUM_BATCHES);

    auto global_start = std::chrono::high_resolution_clock::now();

    for (size_t b = 0; b < NUM_BATCHES; ++b) 
    {
        auto batch_start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < BATCH_SIZE; ++i) 
            book.submit_order(orders_to_submit[b * BATCH_SIZE + i]);
        
        auto batch_end = std::chrono::high_resolution_clock::now();
        
        double batch_ns = std::chrono::duration<double, std::nano>(batch_end - batch_start).count();
        batch_latencies_ns.push_back(batch_ns / BATCH_SIZE);
    }

    auto global_end = std::chrono::high_resolution_clock::now();

    // Compute tail latencies
    std::sort(batch_latencies_ns.begin(), batch_latencies_ns.end());

    double p50   = batch_latencies_ns[static_cast<size_t>(NUM_BATCHES * 0.50)];
    double p90   = batch_latencies_ns[static_cast<size_t>(NUM_BATCHES * 0.90)];
    double p99   = batch_latencies_ns[static_cast<size_t>(NUM_BATCHES * 0.99)];
    double p99_9 = batch_latencies_ns[static_cast<size_t>(NUM_BATCHES * 0.999)];
    double max_lat = batch_latencies_ns.back();

    double total_seconds = std::chrono::duration<double>(global_end - global_start).count();
    double avg_throughput = NUM_ORDERS / total_seconds;

    // Standardized Output Report
    std::cout << " System throughput and latency report:\n";
    std::cout << "  Total Orders:      " << NUM_ORDERS << "\n";
    std::cout << "  Distribution:      " << total_buys << " Buys / " << total_sells << " Sells\n";
    std::cout << "  Total Volume:      " << total_volume << " shares\n\n";

    std::cout << "Performance:\n";
    std::cout << "  Wall Time:         " << std::fixed << std::setprecision(4) << total_seconds << " seconds\n";
    std::cout << "  Avg Throughput:    " << std::setprecision(0) << avg_throughput << " ops/sec\n\n";

    std::cout << "[ LATENCY DISTRIBUTION (Per Order) ]\n";
    std::cout << "  p50 (Median):      " << std::setprecision(2) << p50 << " ns\n";
    std::cout << "  p90:               " << p90 << " ns\n";
    std::cout << "  p99 (Tail):        " << p99 << " ns\n";
    std::cout << "  p99.9 (Extreme):   " << p99_9 << " ns\n";
    std::cout << "  Max:               " << max_lat << " ns\n";

    return 0;
}