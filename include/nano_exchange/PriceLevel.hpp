#pragma once

#include <cassert>
#include <cstdint>
#include "nano_exchange/Order.hpp"

namespace nano_exchange
{
    struct PriceLevel
    {
        Order* first = nullptr;
        Order* last = nullptr;
        uint64_t volume = 0;


        bool is_empty() const
        {
            return first == nullptr && last == nullptr;
        }

        void append_order(Order* order)
        {
            assert(order != nullptr && "Cannot append a null order");
            assert(order->quantity > 0 && "Cannot append an order with 0 quantity");
            assert(order->prev == nullptr && order->next == nullptr && "Order is already linked");

            if(is_empty())
            {
                first = order;
                last = order;
            }
            else
            {
                last->next = order;
                order->prev = last;
                last = order;
            }

            volume += order->quantity;
        }

        void remove_order(Order* order)
        {
            if(is_empty() || order == nullptr)
                return;

            if(order->next == nullptr && order->prev == nullptr && first != order)
                return;

            assert(volume >= order->quantity && "Volume smaller than order quantity");

            volume -= order->quantity;

            if(order->prev != nullptr)
                order->prev->next = order->next;
            else
                first = order->next;

            if(order->next != nullptr)
                order->next->prev = order->prev;
            else
                last = order->prev;

            order->next = nullptr;
            order->prev = nullptr;
        }
    };
}