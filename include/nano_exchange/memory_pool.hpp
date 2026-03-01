#pragma once
#include <vector>
#include <cstddef>
#include <functional>   // for std::less()
#include <stdexcept>   // for std::invalid_argument

namespace nano_exchange
{
template <typename T>
class MemoryPool
{
    private:

    // Using a union to map the object and the free-list index to the same physical bytes
    // - when the position is empty, holds the size_t index of the next free slot
    // - when it's full, the size_t is overwritten by the T object
    union Slot
    {
        T obj;
        size_t nextFreeIndex;

        Slot() {};
        ~Slot() {};
    };

    std::vector<Slot> pool_;
    size_t nextFreeIndex_;

    public:

    explicit MemoryPool(size_t capacity) : pool_(capacity), nextFreeIndex_(0)
    {
        if(capacity == 0)
            throw std::invalid_argument("Capacity must be greater than 0.");

        for(size_t i = 0; i < capacity - 1; i++)
            pool_[i].nextFreeIndex = i + 1;

        pool_[capacity - 1].nextFreeIndex = capacity;  // capacity is the "full" signal
    }

    T* allocate()
    {
        if(nextFreeIndex_ == pool_.size())
            return nullptr;
        
        size_t temp = nextFreeIndex_;
        nextFreeIndex_ = pool_[temp].nextFreeIndex;
        return &pool_[temp].obj;
    }

    void deallocate(T* pointer)
    {
        if(pointer == nullptr)
            return;
        
        Slot* slot_pointer = reinterpret_cast<Slot*>(pointer);

        // Check valid pointer bounds. std::less() avoids UB when comparing pointers
        if(std::less<Slot*>{}(slot_pointer, pool_.data()) || std::greater_equal<Slot*>{}(slot_pointer, pool_.data() + pool_.size()))
            return;

        size_t difference = slot_pointer - pool_.data();
        pool_[difference].nextFreeIndex = nextFreeIndex_;
        nextFreeIndex_ = difference;
    }

    ~MemoryPool() = default;

};
}