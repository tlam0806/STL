#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <vector>

template <typename T>
class PaddedRingBuffer {
private:
    struct alignas(std::hardware_destructive_interference_size) Slot {
        T value;
    };

    std::vector<Slot> buffer;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head{0};
    std::size_t size;

public:
    explicit PaddedRingBuffer(std::size_t size)
        : buffer(size),
          size(size)
    {
        assert((size & (size - 1)) == 0);
    }

    void push(T&& item) {
        std::size_t headValue = head.load(std::memory_order_acquire);
        std::size_t tailValue = tail.load(std::memory_order_relaxed);
        std::size_t nextTail = (tailValue + 1) & (size - 1);
        if (nextTail == headValue) {
            head.wait(headValue, std::memory_order_acquire);
        }
        buffer[tailValue].value = std::move(item);
        tail.store(nextTail, std::memory_order_release);
        tail.notify_one();
    }

    void pop(T& item) {
        std::size_t headValue = head.load(std::memory_order_relaxed);
        std::size_t tailValue = tail.load(std::memory_order_acquire);
        if (headValue == tailValue) {
            tail.wait(tailValue, std::memory_order_acquire);
        }
        item = std::move(buffer[headValue].value);
        std::size_t nextHead = (headValue + 1) & (size - 1);
        head.store(nextHead, std::memory_order_release);
        head.notify_one();
    }
};
