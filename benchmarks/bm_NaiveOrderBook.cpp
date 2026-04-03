#include <benchmark/benchmark.h>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <random>

namespace naive
{

    struct Order
    {
        uint32_t id;
        bool is_buy;
        uint64_t price;
        uint32_t quantity;
    };

    // Standard STL-based Order Book baseline.
    // Provides O(log N) price resolution and O(1) order cancellation,
    // but suffers from heap fragmentation and cache-miss overhead.
    class NaiveOrderBook
    {
    private:
        // O(log N) tree structures for price levels
        std::map<uint64_t, std::list<Order>, std::greater<uint64_t>> bids; 
        std::map<uint64_t, std::list<Order>> asks;                         

        struct OrderLocation
        {
            bool is_buy;
            uint64_t price;
            std::list<Order>::iterator it;
        };

        // O(1) hash map for order tracking and targeted cancellations
        std::unordered_map<uint32_t, OrderLocation> order_map;

    public:
        void submit_order(uint32_t id, bool is_buy, uint64_t price, uint32_t qty)
        {
            Order order{id, is_buy, price, qty};

            if (is_buy)
            {
                // Phase 1: Aggressive matching (Cross the spread)
                auto ask_it = asks.begin();
                while (ask_it != asks.end() && order.quantity > 0 && ask_it->first <= order.price)
                {
                    auto& queue = ask_it->second;
                    auto order_it = queue.begin();
                    
                    while (order_it != queue.end() && order.quantity > 0)
                    {
                        uint32_t trade_qty = std::min(order.quantity, order_it->quantity);
                        order.quantity -= trade_qty;
                        order_it->quantity -= trade_qty;

                        if (order_it->quantity == 0)
                        {
                            order_map.erase(order_it->id);
                            order_it = queue.erase(order_it);
                        } 
                        else
                        {
                            ++order_it;
                        }
                    }

                    if (queue.empty())
                    {
                        ask_it = asks.erase(ask_it);
                    } else
                    {
                        ++ask_it;
                    }
                }

                // Phase 2: Passive resting
                if (order.quantity > 0)
                {
                    bids[order.price].push_back(order);
                    order_map[id] = {true, order.price, std::prev(bids[order.price].end())};
                }
            }
            else
            {
                // Phase 1: Aggressive matching (Cross the spread)
                auto bid_it = bids.begin();
                while (bid_it != bids.end() && order.quantity > 0 && bid_it->first >= order.price)
                {
                    auto& queue = bid_it->second;
                    auto order_it = queue.begin();
                    
                    while (order_it != queue.end() && order.quantity > 0)
                    {
                        uint32_t trade_qty = std::min(order.quantity, order_it->quantity);
                        order.quantity -= trade_qty;
                        order_it->quantity -= trade_qty;

                        if (order_it->quantity == 0)
                        {
                            order_map.erase(order_it->id);
                            order_it = queue.erase(order_it);
                        }
                        else
                        {
                            ++order_it;
                        }
                    }

                    if (queue.empty())
                    {
                        bid_it = bids.erase(bid_it);
                    }
                    else
                    {
                        ++bid_it;
                    }
                }

                // Phase 2: Passive resting
                if (order.quantity > 0)
                {
                    asks[order.price].push_back(order);
                    order_map[id] = {false, order.price, std::prev(asks[order.price].end())};
                }
            }
        }

        // O(1) cancellation via iterator tracking
        void cancel_order(uint32_t id)
        {
            auto loc_it = order_map.find(id);
            if (loc_it == order_map.end()) return;

            const auto& loc = loc_it->second;
            if (loc.is_buy)
            {
                bids[loc.price].erase(loc.it);
                if (bids[loc.price].empty()) bids.erase(loc.price);
            }
            else
            {
                asks[loc.price].erase(loc.it);
                if (asks[loc.price].empty()) asks.erase(loc.price);
            }
            order_map.erase(loc_it);
        }
    };
} // namespace naive

static void BM_NaiveOrderBook_Processing(benchmark::State& state)
{
    naive::NaiveOrderBook book;
    
    // Deterministic PRNG ensures stable and repeatable benchmarking
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> price_dist(1000, 2000);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> type_dist(0, 1); 

    uint32_t order_id = 1;

    for (auto _ : state)
    {
        bool is_buy = (type_dist(rng) == 0);
        uint64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);

        book.submit_order(order_id++, is_buy, price, qty);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_NaiveOrderBook_Processing)->MinTime(2.0);

BENCHMARK_MAIN();