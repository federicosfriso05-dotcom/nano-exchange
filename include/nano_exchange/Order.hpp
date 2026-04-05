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

        Order(uint64_t id_, uint64_t price_, uint32_t quantity_, Side side_)
            : id(id_), price(price_), quantity(quantity_), side(side_) {}
        
        Order() = default;
    };
}