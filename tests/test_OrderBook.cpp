#include <gtest/gtest.h>
#include <random>
#include <vector>
#include "nano_exchange/MemoryPool.hpp"
#include "nano_exchange/OrderBook.hpp"

using namespace nano_exchange;

class OrderBookTest : public ::testing::Test
{
protected:
    static constexpr uint64_t max_price  = 100000;
    static constexpr size_t   max_orders = 10000;

    MemoryPool<Order>*    pool = nullptr;
    OrderBook<NoopCallback>* book = nullptr;

    void SetUp() override
    {
        pool = new MemoryPool<Order>(max_orders);
        book = new OrderBook<>(max_price, max_orders, *pool);
    }

    void TearDown() override
    {
        delete book;
        delete pool;
    }
};

TEST_F(OrderBookTest, SimpleMatch)
{
    // Exact volume and price match between a single buyer and seller.
    Order* sell = pool->create(1, 100, 50, Side::Sell);
    book->submit_order(sell);

    Order* buy = pool->create(2, 100, 50, Side::Buy);
    book->submit_order(buy);

    // The book must be completely empty after a perfect match.
    EXPECT_EQ(book->get_best_ask(), UINT64_MAX);
    EXPECT_EQ(book->get_best_bid(), 0ULL);

    EXPECT_FALSE(book->is_order_active(1));
    EXPECT_FALSE(book->is_order_active(2));
}

TEST_F(OrderBookTest, PriceTimePriority)
{
    // Two asks at the same price level — matching engine must respect FIFO.
    Order* sell1 = pool->create(1, 100, 50, Side::Sell);
    book->submit_order(sell1);

    Order* sell2 = pool->create(2, 100, 50, Side::Sell);
    book->submit_order(sell2);

    Order* buy = pool->create(3, 100, 50, Side::Buy);
    book->submit_order(buy);

    // Older order (ID 1) must be fully filled first.
    EXPECT_FALSE(book->is_order_active(1));

    // Newer order (ID 2) must still be resting.
    EXPECT_TRUE(book->is_order_active(2));

    EXPECT_EQ(book->get_best_ask(), 100ULL);
}

TEST_F(OrderBookTest, ExplicitCancellation)
{
    Order* buy = pool->create(1, 50, 100, Side::Buy);
    book->submit_order(buy);

    EXPECT_EQ(book->get_best_bid(), 50ULL);

    book->cancel_order(1);

    EXPECT_EQ(book->get_best_bid(), 0ULL);
    EXPECT_FALSE(book->is_order_active(1));
}

TEST_F(OrderBookTest, SpreadInvariant)
{
    // Best Bid must NEVER be >= Best Ask: any crossing should have produced a trade.
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> bid_dist(1,  49);
    std::uniform_int_distribution<uint64_t> ask_dist(51, 100);
    std::uniform_int_distribution<uint32_t> qty_dist(1,  100);

    for (size_t i = 1; i <= 5000; ++i)
    {
        const bool     is_buy = (i % 2 == 0);
        const uint64_t price  = is_buy ? bid_dist(rng) : ask_dist(rng);
        const uint32_t qty    = qty_dist(rng);

        Order* o = pool->create(i, price, qty, is_buy ? Side::Buy : Side::Sell);
        book->submit_order(o);

        const uint64_t best_bid = book->get_best_bid();
        const uint64_t best_ask = book->get_best_ask();

        if (best_bid != 0 && best_ask != UINT64_MAX)
            EXPECT_LT(best_bid, best_ask) << "FATAL: Crossed book detected at iteration " << i;
    }
}

TEST_F(OrderBookTest, SweepMultipleLevels)
{
    // Sell-side ladder at 100, 101, 102 (10 qty each).
    book->submit_order(pool->create(1, 100, 10, Side::Sell));
    book->submit_order(pool->create(2, 101, 10, Side::Sell));
    book->submit_order(pool->create(3, 102, 10, Side::Sell));

    // Aggressive buy for 25 qty at 102 — should consume levels 100 and 101 fully
    // and partially fill level 102.
    book->submit_order(pool->create(10, 102, 25, Side::Buy));

    EXPECT_FALSE(book->is_order_active(1));
    EXPECT_FALSE(book->is_order_active(2));
    EXPECT_TRUE(book->is_order_active(3));

    EXPECT_EQ(book->get_best_ask(), 102ULL);
}
