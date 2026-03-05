#pragma once
#include <cstdint>

namespace nano_exchange
{
    enum class Side
    {
        Buy, Sell
    };

    struct Order
    {
        uint64_t id;
        uint64_t price;
        uint32_t quantity;
        Side side;
        Order* next = nullptr;
        Order* prev = nullptr;
    };
}