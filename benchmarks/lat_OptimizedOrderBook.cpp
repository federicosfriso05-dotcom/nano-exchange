#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include "nano_exchange/MemoryPool.hpp"
#include "nano_exchange/OrderBook.hpp"
#include "nano_exchange/Order.hpp"
#
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
  #include <x86intrin.h>
  #define NANO_EXCHANGE_HAS_TSC 1
#else
  #define NANO_EXCHANGE_HAS_TSC 0
#endif

using namespace nano_exchange;

namespace
{
#if NANO_EXCHANGE_HAS_TSC
static inline uint64_t tsc_now_serialized()
{
    // Serialize to reduce out-of-order effects around the measurement.
    // CPUID is heavy, but we're measuring *one* critical section and want stable percentiles.
    unsigned int a, b, c, d;
    a = 0;
    __asm__ __volatile__("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "a"(a));
    return __rdtsc();
}
#endif

[[maybe_unused]] static double calibrate_tsc_hz()
{
#if !NANO_EXCHANGE_HAS_TSC
    return 0.0;
#else
    // Calibrate cycles/sec using a wall clock interval.
    // Keep it short so it runs fast, but long enough to average scheduler noise.
    constexpr auto kSleep = std::chrono::milliseconds(200);

    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t c0 = tsc_now_serialized();
    std::this_thread::sleep_for(kSleep);
    const uint64_t c1 = tsc_now_serialized();
    const auto t1 = std::chrono::steady_clock::now();

    const std::chrono::duration<double> dt = t1 - t0;
    if (dt.count() <= 0.0 || c1 <= c0) return 0.0;
    return static_cast<double>(c1 - c0) / dt.count();
#endif
}

template <typename T>
static inline T percentile_sorted(const std::vector<T>& sorted, double p)
{
    if (sorted.empty()) return T{};
    if (p <= 0.0) return sorted.front();
    if (p >= 1.0) return sorted.back();
    const double idx = p * (static_cast<double>(sorted.size() - 1));
    const size_t i = static_cast<size_t>(idx);
    return sorted[i];
}
} // namespace

struct PreRolledOrder
{
    bool is_buy;
    uint64_t price;
    uint32_t qty;
};

int main()
{
    // --- Engine setup (matches your optimized benchmark intent) ---
    const uint64_t max_price = 100000;
    const size_t max_orders = 10000000;

    MemoryPool<Order> pool(max_orders);
    OrderBook book(max_price, max_orders, pool);

    // Keep the working set tiny and predictable.
    // 8192 * sizeof(PreRolledOrder) is comfortably L1/L2-resident.
    constexpr size_t CACHE_SIZE = 8192;
    std::vector<PreRolledOrder> pre(CACHE_SIZE);

    // Deterministic pseudo-random input stream.
    // (Avoids any OS entropy / timing jitter.)
    uint64_t x = 0x9E3779B97F4A7C15ULL;
    auto next_u64 = [&]() {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        return x * 0x2545F4914F6CDD1DULL;
    };

    for (size_t i = 0; i < CACHE_SIZE; ++i)
    {
        const uint64_t r = next_u64();
        pre[i].is_buy = (r & 1ULL) == 0;
        pre[i].price = 1000 + (static_cast<uint64_t>((r >> 1) % 1001));
        pre[i].qty = 1 + (static_cast<uint32_t>((r >> 20) % 100));
    }

    std::printf("nano-exchange optimized latency microbench\n");
#if NANO_EXCHANGE_HAS_TSC
    // --- Calibrate cycles -> ns (if TSC available) ---
    const double tsc_hz = calibrate_tsc_hz();
    const double ns_per_cycle = (tsc_hz > 0.0) ? (1e9 / tsc_hz) : 0.0;
    if (tsc_hz > 0.0)
        std::printf("TSC: %.3f MHz (%.6f ns/cycle)\n", tsc_hz / 1e6, ns_per_cycle);
    else
        std::printf("TSC: unavailable calibration (printing cycles only)\n");
#else
    std::printf("TSC: not supported on this target (no cycle metrics)\n");
#endif

    // --- Measurement parameters ---
    constexpr size_t WARMUP    = 200000;
    constexpr size_t N         = 2000000;
    constexpr size_t BATCH     = 1000;        // orders per sample
    constexpr size_t N_BATCHES = N / BATCH;   // 2000 samples total

    uint32_t order_id = 1;
    size_t   idx      = 0;

    auto submit_one = [&]() {
        const auto& d = pre[idx];
        Order* o = pool.create();
        *o = Order{order_id, d.price, d.qty, d.is_buy ? Side::Buy : Side::Sell};
        book.submit_order(o);
        idx = (idx + 1) & (CACHE_SIZE - 1);
        order_id++;
        if (order_id >= max_orders) order_id = 1;
    };

    // Warm-up to prime caches and stabilize branches.
    for (size_t i = 0; i < WARMUP; ++i) submit_one();

#if NANO_EXCHANGE_HAS_TSC
    // x86: TSC has sub-ns resolution — measure per order directly.
    std::vector<uint64_t> samples;
    samples.reserve(N);

    for (size_t i = 0; i < N; ++i)
    {
        const uint64_t t0 = tsc_now_serialized();
        submit_one();
        const uint64_t t1 = tsc_now_serialized();
        samples.push_back(t1 - t0);
    }

    std::sort(samples.begin(), samples.end());

    const uint64_t p50  = percentile_sorted(samples, 0.50);
    const uint64_t p90  = percentile_sorted(samples, 0.90);
    const uint64_t p99  = percentile_sorted(samples, 0.99);
    const uint64_t p999 = percentile_sorted(samples, 0.999);

    std::printf("Samples: %zu  (per-order, TSC)\n", samples.size());
    std::printf("p50:   %5" PRIu64 " cycles", p50);
    if (ns_per_cycle > 0.0) std::printf("  (%.2f ns)", p50 * ns_per_cycle);
    std::printf("\n");
    std::printf("p90:   %5" PRIu64 " cycles", p90);
    if (ns_per_cycle > 0.0) std::printf("  (%.2f ns)", p90 * ns_per_cycle);
    std::printf("\n");
    std::printf("p99:   %5" PRIu64 " cycles", p99);
    if (ns_per_cycle > 0.0) std::printf("  (%.2f ns)", p99 * ns_per_cycle);
    std::printf("\n");
    std::printf("p99.9: %5" PRIu64 " cycles", p999);
    if (ns_per_cycle > 0.0) std::printf("  (%.2f ns)", p999 * ns_per_cycle);
    std::printf("\n");

#else
    // ARM/macOS: steady_clock floor is 41.67 ns (24 MHz timer).
    // Measuring one order at a time would give only 0/41/83 ns — meaningless.
    // Batching BATCH orders per sample brings effective resolution to
    // 41.67 / BATCH = ~0.04 ns, giving statistically accurate percentiles.
    std::vector<double> samples;
    samples.reserve(N_BATCHES);

    for (size_t b = 0; b < N_BATCHES; ++b)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < BATCH; ++i) submit_one();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        samples.push_back(ns / static_cast<double>(BATCH));
    }

    std::sort(samples.begin(), samples.end());

    const double p50  = percentile_sorted(samples, 0.50);
    const double p90  = percentile_sorted(samples, 0.90);
    const double p99  = percentile_sorted(samples, 0.99);
    const double p999 = percentile_sorted(samples, 0.999);

    std::printf("Samples: %zu batches x %zu orders  (batch avg, steady_clock)\n",
                N_BATCHES, BATCH);
    std::printf("p50:   %.2f ns\n", p50);
    std::printf("p90:   %.2f ns\n", p90);
    std::printf("p99:   %.2f ns\n", p99);
    std::printf("p99.9: %.2f ns\n", p999);
#endif

    return 0;
}
