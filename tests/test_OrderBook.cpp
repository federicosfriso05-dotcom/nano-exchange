#include <gtest/gtest.h>
#include "nano_exchange/OrderBook.hpp"
#include <vector>
#include <random>

using namespace nano_exchange;

// Test Fixture
class OrderBookTest : public ::testing::Test 
{
protected:
    uint64_t max_price = 100000;
    size_t max_orders = 10000;
    MemoryPool<Order> pool;
    OrderBook* book;

    void SetUp() override
    {
        pool.init(max_orders);
        book = new OrderBook(max_price, max_orders, pool);
    }

    void TearDown() override
    {
        delete book;
    }
};

TEST_F(OrderBookTest, SimpleMatch) {
    // Scenario: Exact volume and price match between a single buyer and seller.
    Order* sell = pool.allocate(1, false, 100, 50);
    book->submit_order(sell);
    
    Order* buy = pool.allocate(2, true, 100, 50);
    book->submit_order(buy);

    // The book must be completely empty after a perfect match.
    EXPECT_EQ(book->get_best_ask(), UINT64_MAX);
    EXPECT_EQ(book->get_best_bid(), 0);
    
    // The orders should no longer be active in the system
    EXPECT_FALSE(book->is_order_active(1));
    EXPECT_FALSE(book->is_order_active(2));
}

TEST_F(OrderBookTest, PriceTimePriority) {
    // Two asks sit at the same price level, the matching engine must respect FIFO execution.
    Order* sell1 = pool.allocate(1, false, 100, 50);
    book->submit_order(sell1);
    
    Order* sell2 = pool.allocate(2, false, 100, 50);
    book->submit_order(sell2);

    // Buyer with only 50 shares
    Order* buy = pool.allocate(3, true, 100, 50);
    book->submit_order(buy);

    // Ensure older order (ID 1) was completely filled and removed
    EXPECT_FALSE(book->is_order_active(1));
    
    // Ensure newer order (ID 2) is still alive
    EXPECT_TRUE(book->is_order_active(2));
    
    // Verify the best ask is still 100 (since order 2 is still there)
    EXPECT_EQ(book->get_best_ask(), 100);
}

TEST_F(OrderBookTest, ExplicitCancellation) {
    // An order is placed and then manually canceled.
    Order* buy = pool.allocate(1, true, 50, 100);
    book->submit_order(buy);
    
    // The book should register the new highest bid
    EXPECT_EQ(book->get_best_bid(), 50);
    
    book->cancel_order(1);
    
    // The book must reflect the removal immediately
    EXPECT_EQ(book->get_best_bid(), 0);
    EXPECT_FALSE(book->is_order_active(1));
}

TEST_F(OrderBookTest, SpreadInvariant) {
    //The Best Bid must NEVER be strictly greater than or equal to the Best Ask.
    // If Bid >= Ask, a trade should have occurred. 
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> bid_dist(1, 49); // Bids only below 50
    std::uniform_int_distribution<uint64_t> ask_dist(51, 100); // Asks only above 50
    std::uniform_int_distribution<uint64_t> qty_dist(1, 100);

    for (size_t i = 1; i <= 5000; ++i) {
        bool is_buy = (i % 2 == 0);
        uint64_t price = is_buy ? bid_dist(rng) : ask_dist(rng);
        
        Order* o = pool.allocate(i, is_buy, price, qty_dist(rng));
        book->submit_order(o);
        
        uint64_t current_bid = book->get_best_bid();
        uint64_t current_ask = book->get_best_ask();
        
        // The core invariant check
        if (current_bid != 0 && current_ask != UINT64_MAX) {
            EXPECT_LT(current_bid, current_ask) << "FATAL: Crossed book detected!";
        }
    }
}

TEST_F(OrderBookTest, SweepMultipleLevels)
{
    // Sell side ladder
    book->submit_order(pool.allocate(1, false, 100, 10));
    book->submit_order(pool.allocate(2, false, 101, 10));
    book->submit_order(pool.allocate(3, false, 102, 10));

    // Big aggressive buy
    book->submit_order(pool.allocate(10, true, 102, 25));

    // Should consume 100, 101, and part of 102
    EXPECT_FALSE(book->is_order_active(1));
    EXPECT_FALSE(book->is_order_active(2));
    EXPECT_TRUE(book->is_order_active(3));

    EXPECT_EQ(book->get_best_ask(), 102);
}