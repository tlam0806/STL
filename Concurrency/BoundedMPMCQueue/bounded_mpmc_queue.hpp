#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <type_traits>

template<std::size_t capacity, typename T> 
class BoundedMpmcQueue {
public : 
    explicit BoundedMpmcQueue() : 
        enqueue_ticket{0},
        dequeue_ticket{0} 
    {
        static_assert(capacity > 1);
        static_assert(std::is_nothrow_move_assignable_v<T>);
        for (std::size_t i = 0; i < capacity; ++i) {
            slots[i].ticket.store(i, std::memory_order_relaxed);
        }
    }

    BoundedMpmcQueue(const BoundedMpmcQueue&) = delete;

    BoundedMpmcQueue& operator=(const BoundedMpmcQueue&) = delete;

    template<class... Args> 
    requires (
        std::is_nothrow_constructible_v<T, Args&&...> &&
        std::is_nothrow_move_assignable_v<T>
    )
    bool try_emplace(Args&&... args) {
        std::size_t enqueue_ticket_value = enqueue_ticket.load(std::memory_order_relaxed);

        while (true) {
            std::size_t slot_index = enqueue_ticket_value % capacity;
            std::size_t slot_ticket = slots[slot_index].ticket.load(std::memory_order_acquire);
            if (slot_ticket < enqueue_ticket_value) {
                return false;
            } 
            if (slot_ticket > enqueue_ticket_value) {
                enqueue_ticket_value = enqueue_ticket.load(std::memory_order_relaxed);
                continue;
            }
            if (enqueue_ticket.compare_exchange_weak(
                    enqueue_ticket_value, 
                    enqueue_ticket_value + 1, 
                    std::memory_order_relaxed, 
                    std::memory_order_relaxed
                )) {
                slots[slot_index].item = T(std::forward<Args>(args)...);
                slots[slot_index].ticket.store(enqueue_ticket_value + 1, std::memory_order_release);
                // release
                return true;
            }
        }
        assert(0); // should not be here 
    }

    bool try_push(const T& value) {
        return try_emplace(value);
    }

    bool try_push(T&& value) {
        return try_emplace(std::move(value));
    }

    bool try_pop(T& output) {
        std::size_t dequeue_ticket_value = dequeue_ticket.load(std::memory_order_relaxed); 

        while (true) {
            std::size_t slot_index =  dequeue_ticket_value % capacity;
            std::size_t slot_ticket = slots[slot_index].ticket.load(std::memory_order_acquire);
            if (slot_ticket < dequeue_ticket_value + 1) {
                return false;
            }
            if (slot_ticket > dequeue_ticket_value + 1) {
                dequeue_ticket_value = dequeue_ticket.load(std::memory_order_relaxed); 
                continue;
            }
            if (dequeue_ticket.compare_exchange_weak(
                    dequeue_ticket_value, 
                    dequeue_ticket_value + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed
                )) {
                output = std::move(slots[slot_index].item);
                slots[slot_index].ticket.store(dequeue_ticket_value + capacity, std::memory_order_release);
                return true;
            } 
        }

        assert(0); // should not be here 
    }

private :
    struct Slot {
        T item;
        std::atomic<std::size_t> ticket;
    };

    Slot slots[capacity];
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> enqueue_ticket;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> dequeue_ticket;
};
