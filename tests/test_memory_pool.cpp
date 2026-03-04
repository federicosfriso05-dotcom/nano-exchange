#include <cstdint>
#include <gtest/gtest.h>
#include "nano_exchange/memory_pool.hpp"


struct DummyOrder 
{
    uint64_t id;
    uint64_t price;
};

// TEST(TestSuiteName, TestName)
TEST(MemoryPoolTest, AllocationAndReuse) 
{
    nano_exchange::MemoryPool<DummyOrder> pool(3);

    DummyOrder* order1 = pool.create();
    DummyOrder* order2 = pool.create();
    DummyOrder* order3 = pool.create();

    ASSERT_NE(order1, nullptr);
    ASSERT_NE(order2, nullptr);
    ASSERT_NE(order3, nullptr);

    // Prove the pool is full
    DummyOrder* order4 = pool.create();
    EXPECT_EQ(order4, nullptr);

    pool.destroy(order2);

    // Allocate a new order. The free list is a Stack (LIFO), so
    // the new order MUST give back the exact memory address that order2 used to have
    DummyOrder* new_order = pool.create();
    EXPECT_EQ(new_order, order2);
}

TEST(MemoryPoolTest, HandlesNullptrDeallocation) 
{
    nano_exchange::MemoryPool<DummyOrder> pool(10);

    EXPECT_NO_FATAL_FAILURE(pool.destroy(nullptr)); 
}

TEST(MemoryPoolTest, RejectsOutOfBoundsPointers) 
{
    nano_exchange::MemoryPool<DummyOrder> pool(10);
    
    DummyOrder fake_order; 
    
    // Attempting to deallocate a pointer not owned by the pool
    EXPECT_NO_FATAL_FAILURE(pool.destroy(&fake_order)); 
    
    // The pool should have ignored it, so allocating a new order should 
    // still give the very first slot (index 0), not the fake pointer.
    DummyOrder* real_order = pool.create();
    EXPECT_NE(real_order, &fake_order); 
}
