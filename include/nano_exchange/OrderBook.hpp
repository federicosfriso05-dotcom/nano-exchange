#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "nano_exchange/MemoryPool.hpp"
#include "nano_exchange/Order.hpp"
#include "nano_exchange/PriceLevel.hpp"

namespace nano_exchange
{
    // Replaces O(N) array scans and O(log N) tree traversals in order to allow the CPU to check 
    // 64 prices simultaneously in a single clock cycle using hardware intrinsics
    struct PriceBitset 
    {
        std::vector<uint64_t> blocks;

        PriceBitset(uint64_t max_price) : blocks((max_price / 64) + 1, 0) {}

        inline void set_bit(uint64_t price)
        {
            blocks[price / 64] |= (1ULL << (price % 64));
        }

        inline void clear_bit(uint64_t price) 
        {
            blocks[price / 64] &= ~(1ULL << (price % 64));
        }

        // Scans up the order book for the lower active ask 
        inline uint64_t find_next_ask(uint64_t start_price, uint64_t max_price) 
        {
            if (start_price > max_price) 
                return max_price + 1;
    
            uint64_t block_idx = start_price / 64;
            uint64_t bit_idx = start_price % 64;
            
            // Mask out alla prices strictly less than start_price
            uint64_t mask = ~((1ULL << bit_idx) - 1);
            uint64_t masked_block = blocks[block_idx] & mask;

            if (masked_block != 0) 
                return (block_idx * 64) + __builtin_ctzll(masked_block);

            for (uint64_t i = block_idx + 1; i < blocks.size(); ++i)
            {
                if (blocks[i] != 0) 
                    return (i * 64) + __builtin_ctzll(blocks[i]);
            }
            return max_price + 1; // Book is empty above start_price
        }

        // Scans down the order book for the highest active bid
        inline uint64_t find_next_bid(uint64_t start_price) 
        {
            if (start_price == UINT64_MAX) return UINT64_MAX;

            uint64_t block_idx = start_price / 64;
            uint64_t bit_idx = start_price % 64;

            // Mask out alla prices strictly greater than start_price
            uint64_t mask = (bit_idx == 63) ? ~0ULL : ((1ULL << (bit_idx + 1)) - 1);
            uint64_t masked_block = blocks[block_idx] & mask;

            if (masked_block != 0) 
                return (block_idx * 64) + (63 - __builtin_clzll(masked_block));

            for (int64_t i = (uint64_t)block_idx - 1; i >= 0; --i)
            {
                if (blocks[i] != 0) 
                    return (i * 64) + (63 - __builtin_clzll(blocks[i]));
            }
            return UINT64_MAX; // Book is empty below start_price
        }
    };


    // An in-memory matching engine designed for ultra-low latency. 
    // Implements O(1) order insertion, cancellation, and price-level resolution.
    // Avoids standard library hashing and dynamic allocation.
    class OrderBook
    {
        private:
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;
            std::vector<Order*> orderVector; // O(1) lookups, defeating std::unordered_map overhead
            PriceBitset bid_bitset;
            PriceBitset ask_bitset;
            uint64_t max_price;
            uint64_t highest_bid;
            uint64_t lowest_ask;
            MemoryPool<Order>& order_pool;

            template <Side S>
            void process_order(Order* order)
            {
                constexpr bool is_buy = (S == Side::Buy);
                
                auto& maker_book    = is_buy ? asks : bids;
                auto& maker_bitset  = is_buy ? ask_bitset : bid_bitset;
                auto& best_price    = is_buy ? lowest_ask : highest_bid;

                while(order->quantity > 0)
                {
                    best_price = is_buy ? maker_bitset.find_next_ask(best_price, max_price) 
                                        : maker_bitset.find_next_bid(best_price);

                    // Spread check invariant: break if liquidity no longer crosses
                    if constexpr (is_buy)
                    {
                        if (best_price > order->price) 
                            break;
                    } 
                    else
                    {
                        if (best_price < order->price || best_price == UINT64_MAX) 
                            break;
                    }

                    Order* stationary_maker = maker_book[best_price].first;
                    uint32_t trade_size = std::min(order->quantity, stationary_maker->quantity);
                    
                    order->quantity -= trade_size;
                    stationary_maker->quantity -= trade_size;
                    maker_book[best_price].volume -= trade_size;

                    // Cleanup depleted resting orders
                    if(stationary_maker->quantity == 0)
                    {
                        maker_book[best_price].remove_order(stationary_maker);
                        orderVector[stationary_maker->id] = nullptr;
                        order_pool.destroy(stationary_maker);
                        
                        if(maker_book[best_price].is_empty())
                        {
                            maker_bitset.clear_bit(best_price);
                        }
                    }
                }
                
                // Add remaining quantity to the book
                if(order->quantity > 0)
                {
                    auto& my_book   = is_buy ? bids : asks;
                    auto& my_bitset = is_buy ? bid_bitset : ask_bitset;
                    
                    my_book[order->price].append_order(order);
                    orderVector[order->id] = order;
                    my_bitset.set_bit(order->price);

                    // Update best bid or ask
                    if constexpr (is_buy)
                    {
                        highest_bid = (highest_bid == UINT64_MAX) ? order->price : std::max(highest_bid, order->price);
                    }
                     else {
                        lowest_ask = std::min(lowest_ask, order->price);
                    }
                }
                else
                {
                    order_pool.destroy(order);
                }
            }

        public:

        OrderBook(uint64_t maxPrice, size_t max_orders, MemoryPool<Order>& pool) : bids(maxPrice + 1), asks(maxPrice + 1), 
            orderVector(max_orders + 1, nullptr), bid_bitset(maxPrice), ask_bitset(maxPrice), max_price(maxPrice), 
            highest_bid(UINT64_MAX), lowest_ask(maxPrice + 1), order_pool(pool)
        {
            if(max_price == 0)
                throw std::invalid_argument("Max price must be greaten than 0");
        }

        void cancel_order(uint64_t order_id)
        {
            if(order_id >= orderVector.size() || orderVector[order_id] == nullptr)
                throw std::invalid_argument("Order not found!");
            
            Order* target = orderVector[order_id];
            uint64_t price = target->price;

            if(target->side == Side::Buy)
            {
                bids[target->price].remove_order(target);
                
                if(bids[price].is_empty())
                {
                    bid_bitset.clear_bit(price);

                    if(price == highest_bid)
                        highest_bid = bid_bitset.find_next_bid(price);
                }
            }
            else
            {
                asks[target->price].remove_order(target);

                if(asks[price].is_empty())
                {
                    ask_bitset.clear_bit(price);

                    if(price == lowest_ask)
                        lowest_ask = ask_bitset.find_next_ask(price, max_price);
                }
            }

            orderVector[order_id] = nullptr;
            order_pool.destroy(target);
        }

        void submit_order(Order* order)
        {
            if(order == nullptr || order->price > max_price || order->quantity == 0)
                throw std::invalid_argument("Order not valid");

            if(order->id >= orderVector.size() || orderVector[order->id] != nullptr)
                throw std::invalid_argument("Order ID not valid or already exists");

            if(order->side == Side::Buy)
                process_order<Side::Buy>(order);
            else
                process_order<Side::Sell>(order);
        }
    };
}