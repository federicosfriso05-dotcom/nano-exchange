#pragma once
#include <cstdint>
#include "nano_exchange/Order.hpp"

namespace nano_exchange
{
    struct TradeEvent
    {
        uint64_t aggressor_id;
        uint64_t maker_id;
        uint64_t price;
        uint32_t quantity;
        Side     aggressor_side;
    };
}
