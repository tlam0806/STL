#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <vector>

template<typename T> 
class MPMCQueue {
public : 
    explicit MPMCQueue(std::size_t capacity) : 
        
        capacity{capacity},
        enqueue_ticket{0},
        dequeue_ticket{0} {
        assert(capacity > 1);
        slots.reserve(capacity);
        const defaultT = T();
        for (std::size_t i = 0; i < capacity; ++i) {
            slots.emplace_back(defaultT, i);
        }
    }

    template<Args... args> 
    bool try_emplace(Args&&... args) {
        std::size_t enqueue_ticket_value = enqueue_ticket.load(std::memory_order_relaxed);
        std::size_t slot_index = enqueue_ticket % capacity;
        if (slots[slot_index].ticket.load(std::memory_order_acquire) != enqueue_ticket_value) {
            return false;
        }
        if (enqueue_ticket.compare_exchange_weak(
                enqueue_ticket_value, 
                enqueue_ticket_value + 1, 
                std::memory_order_acquire, 
                std::memory_order_relaxed
            )) {
            slots[slot_index].item = T(std::forward<Args>(args)...);
            slots[slot_index].sequence.fetch_add(1, std::memory_order_release);
            // release
            return true;
        } else {
            return false;
        }
    }

    bool try_push(const T& value) {
        return try_emplace(value);
    }

    bool try_push(T&& value) {
        return try_emplace(std::move(value));
    }

    bool try_pop(T& output) {
        std::size_t dequeue_ticket_value = dequeue_ticket.load(std::memory_order_relaxed); // fixed
        std::size_t slot_index =  dequeue_ticket_value % capacity;
        if (slots[slot_index].ticket.load(std::memory_order_acquire) != dequeue_ticket_value + 1) {
            return false;
        }
        if (dequeue_ticket.compare_exchange_weak(
                dequeue_ticket_value, 
                dequeue_ticket_value + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed
            )) {
            output = std::move_if_noexcept(slots[slot_index]);
            slots[slot_index].fetch_add(capacity - 1, std::memory_order_release);
            return true;
        } else {
            return false;
        }
    }

private :
    struct Slot {
        T item;
        std::atomic<std::size_t> ticket;
    }

    std::vector<Slot> slots;
    std::size_t capacity;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> enqueue_ticket;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> dequeue_ticket;
};
