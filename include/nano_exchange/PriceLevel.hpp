#pragma once

#include <cassert>
#include <cstdint>
#include "nano_exchange/order.hpp"

namespace nano_exchange
{
    struct PriceLevel
    {
        Order* first = nullptr;
        Order* last = nullptr;
        uint32_t volume = 0;


        bool is_empty() const
        {
            return first == nullptr;
        }

        void append_order(Order* order)
        {
            assert(order != nullptr && "Cannot append a null order");

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