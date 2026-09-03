#include "CircularQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <string_view>

namespace {

[[noreturn]] void fail(std::string_view check,
                       std::size_t step,
                       std::size_t index = 0) {
    std::cerr << "FAIL: " << check << " (step=" << step
              << ", index=" << index << ")\n";
    std::abort();
}

void verify(CircularQueue<int>& actual,
            const std::deque<int>& expected,
            std::size_t step) {
    if (actual.empty() != expected.empty()) {
        fail("empty", step);
    }

    if (actual.size() != expected.size()) {
        fail("size", step);
    }

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            fail("operator[]", step, i);
        }
    }

    std::size_t i = 0;
    for (int value : actual) {
        if (i >= expected.size() || value != expected[i]) {
            fail("iterator", step, i);
        }
        ++i;
    }

    if (i != expected.size()) {
        fail("iterator count", step, i);
    }
}

void deterministic_stress() {
    CircularQueue<int> actual;
    std::deque<int> expected;

    constexpr std::size_t operations = 100'000;
    for (std::size_t step = 0; step < operations; ++step) {
        const int value = static_cast<int>(step);

        switch (step % 4) {
        case 0:
            actual.push_back(value);
            expected.push_back(value);
            break;
        case 1:
            actual.push_front(value);
            expected.push_front(value);
            break;
        case 2: {
            const int lvalue = -value;
            actual.push_back(lvalue);
            expected.push_back(lvalue);
            break;
        }
        default: {
            int& inserted = actual.emplace_front(-value);
            expected.emplace_front(-value);
            if (inserted != expected.front()) {
                fail("emplace_front return", step);
            }
            break;
        }
        }

        if ((step + 1) % 4096 == 0) {
            verify(actual, expected, step);
        }
    }

    verify(actual, expected, operations);
    if (actual.front() != expected.front()) {
        fail("front", operations);
    }
    if (actual.back() != expected.back()) {
        fail("back", operations);
    }
}

void randomized_stress() {
    constexpr std::uint64_t seed = 0xC1AC'0A11'2026ULL;
    constexpr std::size_t operations = 250'000;

    std::mt19937_64 random(seed);
    std::uniform_int_distribution<int> value_distribution(-1'000'000'000,
                                                           1'000'000'000);
    std::uniform_int_distribution<int> operation_distribution(0, 3);

    CircularQueue<int> actual;
    std::deque<int> expected;

    for (std::size_t step = 0; step < operations; ++step) {
        const int value = value_distribution(random);

        switch (operation_distribution(random)) {
        case 0:
            actual.push_front(value);
            expected.push_front(value);
            break;
        case 1:
            actual.push_back(value);
            expected.push_back(value);
            break;
        case 2: {
            int& inserted = actual.emplace_front(value);
            expected.emplace_front(value);
            if (inserted != expected.front()) {
                fail("random emplace_front return", step);
            }
            break;
        }
        default: {
            int& inserted = actual.emplace_back(value);
            expected.emplace_back(value);
            if (inserted != expected.back()) {
                fail("random emplace_back return", step);
            }
            break;
        }
        }

        if ((step + 1) % 8192 == 0) {
            verify(actual, expected, step);
        }
    }

    verify(actual, expected, operations);
    if (actual.front() != expected.front()) {
        fail("random front", operations);
    }
    if (actual.back() != expected.back()) {
        fail("random back", operations);
    }
}

} // namespace

int main() {
    deterministic_stress();
    randomized_stress();
    std::cout << "CircularQueue stress test passed\n";
}
