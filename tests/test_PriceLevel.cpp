#include <gtest/gtest.h>
#include "nano_exchange/PriceLevel.hpp"
#include "nano_exchange/Order.hpp"

using namespace nano_exchange;

TEST(PriceLevelTest, IsEmptyOnCreation)
{
    PriceLevel level;
    EXPECT_TRUE(level.is_empty());
    EXPECT_EQ(level.volume, 0);
    EXPECT_EQ(level.first, nullptr);
    EXPECT_EQ(level.last, nullptr);
}

TEST(PriceLevelTest, AppendFirstOrder)
{
    PriceLevel level;
    Order order1 {nullptr, nullptr, 1, 150, 100, Side::Buy};

    level.append_order(&order1);

    EXPECT_FALSE(level.is_empty());
    EXPECT_EQ(level.volume, 100);
    EXPECT_EQ(level.first, &order1);
    EXPECT_EQ(level.last, &order1);
}

TEST(PriceLevelTest, RemoveMiddleOrder)
{
    PriceLevel level;
    Order order1{nullptr, nullptr, 1, 200, 100, Side::Buy};
    Order order2{nullptr, nullptr, 2, 250, 200, Side::Buy};
    Order order3{nullptr, nullptr, 3, 100, 300, Side::Buy};

    level.append_order(&order1);
    level.append_order(&order2);
    level.append_order(&order3);

    level.remove_order(&order2);

    EXPECT_EQ(level.first, &order1);
    EXPECT_EQ(level.last, &order3);
    EXPECT_EQ(order1.next, &order3);
    EXPECT_EQ(order3.next, nullptr);
    EXPECT_EQ(order3.prev, &order1);
    EXPECT_EQ(order2.next, nullptr);
    EXPECT_EQ(order2.prev, nullptr);
    EXPECT_EQ(level.volume, 400);
}

TEST(PriceLevelTest, RemoveFirstOrder)
{
    PriceLevel level;
    Order order1{nullptr, nullptr, 1, 200, 100, Side::Buy};
    Order order2{nullptr, nullptr, 2, 250, 200, Side::Buy};
    Order order3{nullptr, nullptr, 3, 100, 300, Side::Buy};

    level.append_order(&order1);
    level.append_order(&order2);
    level.append_order(&order3);

    level.remove_order(&order1);

    EXPECT_EQ(level.first, &order2);
    EXPECT_EQ(level.last, &order3);
    EXPECT_EQ(order2.next, &order3);
    EXPECT_EQ(order2.prev, nullptr);
    EXPECT_EQ(order3.next, nullptr);
    EXPECT_EQ(order3.prev, &order2);
    EXPECT_EQ(order1.next, nullptr);
    EXPECT_EQ(order1.prev, nullptr);
    EXPECT_EQ(level.volume, 500);
}

TEST(PriceLevelTest, RemoveLastOrder)
{
    PriceLevel level;
    Order order1{nullptr, nullptr, 1, 200, 100, Side::Buy};
    Order order2{nullptr, nullptr, 2, 250, 200, Side::Buy};
    Order order3{nullptr, nullptr, 3, 100, 300, Side::Buy};

    level.append_order(&order1);
    level.append_order(&order2);
    level.append_order(&order3);

    level.remove_order(&order3);

    EXPECT_EQ(level.first, &order1);
    EXPECT_EQ(level.last, &order2);
    EXPECT_EQ(order2.next, nullptr);
    EXPECT_EQ(order2.prev, &order1);
    EXPECT_EQ(order3.next, nullptr);
    EXPECT_EQ(order3.prev, nullptr);
    EXPECT_EQ(order1.next, &order2);
    EXPECT_EQ(order1.prev, nullptr);
    EXPECT_EQ(level.volume, 300);
}

TEST(PriceLevelTest, RemoveUniqueOrder)
{
    PriceLevel level;
    Order order1{nullptr, nullptr, 1, 200, 100, Side::Buy};

    level.append_order(&order1);

    level.remove_order(&order1);

    EXPECT_EQ(level.first, nullptr);
    EXPECT_EQ(level.last, nullptr);
    EXPECT_EQ(level.volume, 0);
}