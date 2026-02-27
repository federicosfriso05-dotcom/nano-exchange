#include <gtest/gtest.h>
#include "nano_exchange/memory_pool.hpp"
#include <cstdint>

// A dummy struct to represent an Order for our tests
struct DummyOrder {
    uint64_t id;
    uint64_t price;
};

// TEST(TestSuiteName, TestName)
TEST(MemoryPoolTest, AllocationAndReuse) {
    // 1. Create a pool with a tiny capacity of 3
    nano_exchange::MemoryPool<DummyOrder> pool(3);

    // 2. Allocate 3 orders (fill the pool)
    DummyOrder* order1 = pool.allocator();
    DummyOrder* order2 = pool.allocator();
    DummyOrder* order3 = pool.allocator();

    // ASSERT_NE means "Assert Not Equal". We prove they actually got memory.
    ASSERT_NE(order1, nullptr);
    ASSERT_NE(order2, nullptr);
    ASSERT_NE(order3, nullptr);

    // 3. Prove the pool is full! The 4th allocation MUST return nullptr.
    DummyOrder* order4 = pool.allocator();
    EXPECT_EQ(order4, nullptr);

    // 4. THE CRITICAL TEST: Cancel (deallocate) order2
    pool.deallocator(order2);

    // 5. Allocate a new order. Because our free list is a Stack (LIFO),
    // it MUST give us the exact memory address that order2 used to have!
    DummyOrder* new_order = pool.allocator();
    
    // EXPECT_EQ means "Expect Equal". This proves our O(1) reuse logic is flawless.
    EXPECT_EQ(new_order, order2);
}

    // TEST: Can it survive being handed a nullptr?
TEST(MemoryPoolTest, HandlesNullptrDeallocation) {
    nano_exchange::MemoryPool<DummyOrder> pool(10);
    // EXPECT_NO_FATAL_FAILURE ensures the program doesn't crash
    EXPECT_NO_FATAL_FAILURE(pool.deallocator(nullptr)); 
}

// TEST: Can it survive being handed a pointer from outside the pool?
TEST(MemoryPoolTest, RejectsOutOfBoundsPointers) {
    nano_exchange::MemoryPool<DummyOrder> pool(10);
    
    // Create an order on the standard stack (NOT in the pool)
    DummyOrder fake_order; 
    
    // Try to trick the pool into deallocating it
    EXPECT_NO_FATAL_FAILURE(pool.deallocator(&fake_order)); 
    
    // The pool should have ignored it, so allocating a new order should 
    // still give us the very first slot (index 0), not the fake pointer.
    DummyOrder* real_order = pool.allocator();
    EXPECT_NE(real_order, &fake_order); 
}
