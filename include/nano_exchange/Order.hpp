#pragma once
#include <cstdint>

namespace nano_exchange
{
    enum class Side: uint8_t
    {
        Buy, Sell
    };

    struct Order
    {
        // Cache optimization
        Order* next = nullptr;
        Order* prev = nullptr;
        uint64_t id;
        uint64_t price;
        uint32_t quantity;
        Side side;
    };
}