#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "nano_exchange/MemoryPool.hpp"
#include "nano_exchange/Order.hpp"
#include "nano_exchange/OrderBook.hpp"
#include "nano_exchange/SPSCQueue.hpp"
#include "nano_exchange/TradeEvent.hpp"

using namespace nano_exchange;

static constexpr uint64_t MAX_PRICE      = 100000;
static constexpr size_t   MAX_ORDERS     = 1000000;
static constexpr size_t   NUM_ORDERS     = 1000000;
static constexpr size_t   BATCH_SIZE     = 1000;
static constexpr size_t   NUM_BATCHES    = NUM_ORDERS / BATCH_SIZE;
static constexpr size_t   QUEUE_CAPACITY = 4096;   // must be power of 2


int main()
{
    SPSCQueue<OrderDescriptor, QUEUE_CAPACITY> order_queue;
    SPSCQueue<TradeEvent,      QUEUE_CAPACITY> trade_queue;

    std::atomic<bool> gateway_done{false};
    std::atomic<bool> matching_done{false};

    // Written only by the gateway thread, read after join — no need for atomics.
    size_t   total_buys   = 0;
    size_t   total_sells  = 0;
    uint64_t total_volume = 0;

    // Written only by the matching thread, read after join.
    std::vector<double> latency_samples;
    latency_samples.reserve(NUM_BATCHES);

    std::atomic<uint64_t> trades_executed{0};
    std::atomic<uint64_t> volume_matched{0};

    // Reporter thread
    std::thread reporter([&]()
    {
        TradeEvent ev;
        while (!matching_done.load(std::memory_order_acquire) || !trade_queue.empty())
        {
            if (trade_queue.pop(ev))
            {
                trades_executed.fetch_add(1, std::memory_order_relaxed);
                volume_matched.fetch_add(ev.quantity, std::memory_order_relaxed);
            }
        }
    });

    // Matching thread 
    std::thread matching([&]()
    {
        MemoryPool<Order> pool(MAX_ORDERS);

        auto on_trade = [&](const TradeEvent& ev)
        {
            while (!trade_queue.push(ev)) {}
        };

        OrderBook book(MAX_PRICE, MAX_ORDERS, pool, on_trade);

        OrderDescriptor desc;
        size_t orders_processed = 0;
        auto batch_start = std::chrono::high_resolution_clock::now();

        while (!gateway_done.load(std::memory_order_acquire) || !order_queue.empty())
        {
            if (!order_queue.pop(desc))
                continue;

            Order* o = pool.create(desc.id, desc.price, desc.quantity, desc.side);
            if (o)
                book.submit_order(o);

            ++orders_processed;
            if (orders_processed % BATCH_SIZE == 0)
            {
                auto now = std::chrono::high_resolution_clock::now();
                double batch_ns = std::chrono::duration<double, std::nano>(now - batch_start).count();
                latency_samples.push_back(batch_ns / BATCH_SIZE);
                batch_start = now;
            }
        }

        matching_done.store(true, std::memory_order_release);
    });

    // Gateway (main thread): generates random orders and pushes them to the matching thread via SPSC queue.
    uint64_t x = 0x9E3779B97F4A7C15ULL;
    auto next_u64 = [&]() noexcept -> uint64_t
    {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        return x * 0x2545F4914F6CDD1DULL;
    };

    const auto t_start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 1; i <= NUM_ORDERS; ++i)
    {
        const uint64_t r     = next_u64();
        const bool     is_buy = (r & 1ULL) == 0;
        const uint64_t price  = 49800 + (r >> 1) % 401;
        const uint32_t qty    = static_cast<uint32_t>(1 + (r >> 20) % 100);

        if (is_buy) ++total_buys; else ++total_sells;
        total_volume += qty;

        const OrderDescriptor desc{i, price, qty, is_buy ? Side::Buy : Side::Sell};
        while (!order_queue.push(desc)) {}
    }

    gateway_done.store(true, std::memory_order_release);

    matching.join();
    reporter.join();

    const auto   t_end          = std::chrono::high_resolution_clock::now();
    const double total_seconds  = std::chrono::duration<double>(t_end - t_start).count();
    const double avg_throughput = NUM_ORDERS / total_seconds;

    // Compute latency percentiles from matching-thread samples
    std::sort(latency_samples.begin(), latency_samples.end());

    auto percentile = [&](double p) -> double
    {
        if (latency_samples.empty()) return 0.0;
        const size_t idx = static_cast<size_t>(NUM_BATCHES * p);
        return latency_samples[std::min(idx, latency_samples.size() - 1)];
    };

    const double p50    = percentile(0.50);
    const double p90    = percentile(0.90);
    const double p99    = percentile(0.99);
    const double p99_9  = percentile(0.999);
    const double max_lat = latency_samples.empty() ? 0.0 : latency_samples.back();

    // Final Report
    std::cout << " System throughput and latency report:\n";
    std::cout << "  Total Orders:      " << NUM_ORDERS   << "\n";
    std::cout << "  Distribution:      " << total_buys   << " Buys / " << total_sells << " Sells\n";
    std::cout << "  Total Volume:      " << total_volume << " shares\n\n";

    std::cout << "Performance:\n";
    std::cout << "  Wall Time:         " << std::fixed << std::setprecision(4) << total_seconds  << " seconds\n";
    std::cout << "  Avg Throughput:    " << std::setprecision(0) << avg_throughput << " ops/sec\n\n";

    std::cout << "[ LATENCY DISTRIBUTION (Per Order) ]\n";
    std::cout << "  p50 (Median):      " << std::setprecision(2) << p50    << " ns\n";
    std::cout << "  p90:               "                         << p90    << " ns\n";
    std::cout << "  p99 (Tail):        "                         << p99    << " ns\n";
    std::cout << "  p99.9 (Extreme):   "                         << p99_9  << " ns\n";
    std::cout << "  Max:               "                         << max_lat << " ns\n";

    return 0;
}
