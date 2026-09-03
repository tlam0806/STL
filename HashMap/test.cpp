#include "HashMap.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

[[noreturn]] void fail(
    const char* operation,
    std::size_t step = 0,
    int key = 0)
{
    std::cerr << "FAIL: " << operation
              << " (step=" << step << ", key=" << key << ")\n";
    std::abort();
}

void check(
    bool condition,
    const char* operation,
    std::size_t step = 0,
    int key = 0)
{
    if (!condition) {
        fail(operation, step, key);
    }
}

struct ConstantHash {
    std::size_t operator()(int) const noexcept {
        return 0;
    }
};

struct CountingHash {
    static inline std::size_t calls = 0;

    std::size_t operator()(int key) const noexcept {
        ++calls;
        return std::hash<int>{}(key);
    }
};

struct Tracked {
    static inline int alive = 0;

    int value = 0;

    Tracked() {
        ++alive;
    }

    explicit Tracked(int value_) : value{value_} {
        ++alive;
    }

    Tracked(const Tracked& other) : value{other.value} {
        ++alive;
    }

    Tracked(Tracked&& other) noexcept : value{other.value} {
        ++alive;
    }

    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) = default;

    ~Tracked() {
        --alive;
    }
};

template<typename Hash>
void check_against_reference(
    HashMap<int, int, Hash>& actual,
    const std::unordered_map<int, int>& expected,
    std::size_t step)
{
    check(actual.size() == expected.size(), "size mismatch", step);
    check(actual.empty() == expected.empty(), "empty mismatch", step);

    for (const auto& [key, value] : expected) {
        auto found = actual.find(key);
        check(found != actual.end(), "missing key", step, key);
        check(found->first == key, "find returned wrong key", step, key);
        check(found->second == value, "wrong mapped value", step, key);
    }

    std::unordered_set<int> visited;
    std::size_t count = 0;

    for (auto it = actual.begin(); it != actual.end(); ++it) {
        ++count;
        check(count <= expected.size(), "iterator cycle or extra element", step);

        const auto reference_it = expected.find(it->first);
        check(reference_it != expected.end(), "iteration returned unknown key", step, it->first);
        check(reference_it->second == it->second, "iteration returned wrong value", step, it->first);
        check(visited.insert(it->first).second, "iteration returned duplicate key", step, it->first);
    }

    check(count == expected.size(), "iteration ended early", step);
}

void test_empty_map() {
    HashMap<int, int> map;

    check(map.empty(), "new map not empty");
    check(map.size() == 0, "new map size");
    check(map.begin() == map.end(), "empty begin/end");
    check(map.find(123) == map.end(), "find in empty map", 0, 123);
}

void test_insert_find_and_subscript() {
    HashMap<int, std::string> map;

    auto first = map.insert({1, "one"});
    check(first->first == 1, "rvalue insert key", 0, 1);
    check(first->second == "one", "rvalue insert value", 0, 1);
    check(map.size() == 1, "size after rvalue insert");

    std::pair<const int, std::string> second{2, "two"};
    auto second_it = map.insert(second);
    check(second_it->second == "two", "lvalue insert", 0, 2);
    check(map.size() == 2, "size after lvalue insert");

    map.insert({1, "ONE"});
    check(map.size() == 2, "duplicate insert changed size", 0, 1);
    check(map.find(1)->second == "ONE", "duplicate insert did not update", 0, 1);

    int lvalue_key = 3;
    check(map[lvalue_key].empty(), "lvalue subscript not default initialized", 0, lvalue_key);
    map[lvalue_key] = "three";
    check(map[lvalue_key] == "three", "lvalue subscript assignment", 0, lvalue_key);

    map[4] = "four";
    check(map[4] == "four", "rvalue subscript assignment", 0, 4);
    check(map.size() == 4, "subscript size");
}

void test_collisions_and_iteration() {
    HashMap<int, int, ConstantHash> map;
    std::unordered_map<int, int> expected;

    for (int key = 0; key < 200; ++key) {
        map.insert({key, key * 11});
        expected.insert_or_assign(key, key * 11);
    }

    check_against_reference(map, expected, 200);

    for (int key = 0; key < 200; key += 3) {
        map[key] = -key;
        expected[key] = -key;
    }

    check_against_reference(map, expected, 201);
}

void test_automatic_rehash() {
    constexpr int element_count = 2'000;

    CountingHash::calls = 0;
    HashMap<int, int, CountingHash> map;
    std::unordered_map<int, int> expected;
    std::vector<std::pair<int, int*>> saved_addresses;
    bool observed_redistribution = false;

    for (int key = 0; key < element_count; ++key) {
        const std::size_t calls_before = CountingHash::calls;
        auto inserted = map.insert({key, key * 17});
        const std::size_t calls_for_insert = CountingHash::calls - calls_before;

        if (calls_for_insert > 1) {
            observed_redistribution = true;
        }

        expected.insert_or_assign(key, key * 17);
        check(inserted->first == key, "rehash insertion returned wrong key", static_cast<std::size_t>(key), key);
        check(inserted->second == key * 17, "rehash insertion returned wrong value", static_cast<std::size_t>(key), key);

        if (key < 12) {
            saved_addresses.push_back({key, &inserted->second});
        }
    }

    check(observed_redistribution, "automatic rehash was not observed");
    check_against_reference(map, expected, element_count);

    for (const auto& [key, old_address] : saved_addresses) {
        auto found = map.find(key);
        check(found != map.end(), "rehash lost referenced key", element_count, key);
        check(&found->second == old_address, "rehash changed element address", element_count, key);
        check(*old_address == key * 17, "rehash invalidated element reference", element_count, key);
    }

    const std::size_t size_before_duplicate = map.size();
    const std::size_t calls_before_duplicate = CountingHash::calls;
    map.insert({17, -17});
    const std::size_t duplicate_hash_calls = CountingHash::calls - calls_before_duplicate;

    check(map.size() == size_before_duplicate, "duplicate insertion changed size after rehash", 0, 17);
    check(map.find(17)->second == -17, "duplicate insertion value after rehash", 0, 17);
    check(duplicate_hash_calls == 1, "duplicate insertion triggered rehash", 0, 17);
}

void test_explicit_rehash() {
    HashMap<int, int> map;
    std::unordered_map<int, int> expected;

    for (int key = 0; key < 80; ++key) {
        map.insert({key, key + 1'000});
        expected.insert_or_assign(key, key + 1'000);
    }

    int* saved_address = &map.find(7)->second;
    map.rehash(257);

    check(map.size() == expected.size(), "explicit rehash changed size");
    check_against_reference(map, expected, 257);
    check(&map.find(7)->second == saved_address, "explicit rehash changed element address", 0, 7);
    check(*saved_address == 1'007, "explicit rehash invalidated reference", 0, 7);

    map.rehash(257);
    check_against_reference(map, expected, 258);

    HashMap<int, int> empty;
    empty.rehash(32);
    check(empty.empty(), "explicit rehash changed empty map");
    check(empty.begin() == empty.end(), "explicit rehash broke empty iteration");
}

void test_erase_by_key() {
    HashMap<int, int, ConstantHash> map;
    map.insert({1, 10});
    map.insert({2, 20});
    map.insert({3, 30});

    check(map.erase(3) == 1, "erase bucket head", 0, 3);
    check(map.find(3) == map.end(), "erased head still found", 0, 3);
    check(map.size() == 2, "size after erasing head", 0, 3);

    check(map.erase(1) == 1, "erase bucket tail", 0, 1);
    check(map.find(1) == map.end(), "erased tail still found", 0, 1);

    check(map.erase(2) == 1, "erase only node", 0, 2);
    check(map.empty(), "map not empty after erasing all");
    check(map.erase(999) == 0, "erase missing key", 0, 999);
}

void test_erase_by_iterator() {
    HashMap<int, int, ConstantHash> map;
    map.insert({1, 10});
    map.insert({2, 20});
    map.insert({3, 30});
    map.insert({4, 40});

    auto victim = map.find(2);
    auto expected_next = victim;
    ++expected_next;

    const bool should_return_end = expected_next == map.end();
    const int expected_next_key = should_return_end ? 0 : expected_next->first;
    auto returned = map.erase(victim);

    check(map.find(2) == map.end(), "iterator erase left key", 0, 2);
    check(map.size() == 3, "size after iterator erase", 0, 2);
    check((returned == map.end()) == should_return_end, "iterator erase returned wrong position", 0, 2);
    if (!should_return_end) {
        check(returned->first == expected_next_key, "iterator erase returned wrong key", 0, 2);
    }
}

void test_copy_operations() {
    HashMap<int, std::string> original;
    original.insert({1, "one"});
    original.insert({2, "two"});
    original.insert({3, "three"});

    HashMap<int, std::string> copied{original};
    check(copied.size() == original.size(), "copy constructor size");
    copied.insert({2, "changed"});
    copied.erase(1);
    check(original.find(1) != original.end(), "copy constructor shared erased node", 0, 1);
    check(original.find(2)->second == "two", "copy constructor shared value", 0, 2);

    HashMap<int, std::string> assigned;
    assigned.insert({99, "old"});
    assigned = original;
    check(assigned.size() == original.size(), "copy assignment size");
    check(assigned.find(99) == assigned.end(), "copy assignment kept old key", 0, 99);
    assigned.insert({3, "assigned change"});
    check(original.find(3)->second == "three", "copy assignment shared value", 0, 3);

    HashMap<int, std::string>* original_self = &original;
    original = *original_self;
    check(original.size() == 3, "self copy assignment size");
    check(original.find(1) != original.end(), "self copy assignment lost values", 0, 1);
}

void test_move_operations() {
    HashMap<int, int> source;
    source.insert({1, 10});
    source.insert({2, 20});

    HashMap<int, int> moved{std::move(source)};
    check(moved.size() == 2, "move constructor size");
    check(moved.find(1)->second == 10, "move constructor value", 0, 1);
    source[7] = 70;
    check(source.find(7)->second == 70, "moved-from reuse", 0, 7);

    HashMap<int, int> assigned;
    assigned.insert({99, 990});
    assigned = std::move(moved);
    check(assigned.size() == 2, "move assignment size");
    check(assigned.find(1)->second == 10, "move assignment value", 0, 1);
    check(assigned.find(99) == assigned.end(), "move assignment kept old key", 0, 99);

    HashMap<int, int>* assigned_self = &assigned;
    assigned = std::move(*assigned_self);
    check(assigned.size() == 2, "self move assignment size");
    check(assigned.find(2)->second == 20, "self move assignment lost values", 0, 2);
}

void test_value_lifetime() {
    check(Tracked::alive == 0, "tracked initial lifetime");

    {
        HashMap<int, Tracked> map;
        map[1].value = 10;
        map[2].value = 20;
        map.insert({3, Tracked{30}});
        check(Tracked::alive == 3, "tracked live elements");
    }

    check(Tracked::alive == 0, "tracked final lifetime");
}

void test_randomized() {
    constexpr std::size_t operation_count = 50'000;
    constexpr unsigned seed = 0xC1A0'2026U;

    HashMap<int, int> actual;
    std::unordered_map<int, int> expected;
    std::mt19937 generator{seed};
    std::uniform_int_distribution<int> key_distribution{-300, 300};
    std::uniform_int_distribution<int> value_distribution{-100'000, 100'000};
    std::uniform_int_distribution<int> operation_distribution{0, 3};

    for (std::size_t step = 0; step < operation_count; ++step) {
        const int key = key_distribution(generator);
        const int value = value_distribution(generator);

        switch (operation_distribution(generator)) {
        case 0:
            actual.insert({key, value});
            expected.insert_or_assign(key, value);
            break;

        case 1:
            actual[key] = value;
            expected[key] = value;
            break;

        case 2: {
            const std::size_t actual_removed = actual.erase(key);
            const std::size_t expected_removed = expected.erase(key);
            check(actual_removed == expected_removed, "random erase result", step, key);
            break;
        }

        case 3: {
            auto actual_it = actual.find(key);
            auto expected_it = expected.find(key);
            check(
                (actual_it == actual.end()) == (expected_it == expected.end()),
                "random find presence",
                step,
                key);
            if (expected_it != expected.end()) {
                check(actual_it->second == expected_it->second, "random find value", step, key);
            }
            break;
        }
        }

        if (step % 251 == 0) {
            check_against_reference(actual, expected, step);
        }
    }

    check_against_reference(actual, expected, operation_count);
}

} // namespace

int main() {
    test_empty_map();
    test_insert_find_and_subscript();
    test_collisions_and_iteration();
    test_automatic_rehash();
    test_explicit_rehash();
    test_erase_by_key();
    test_erase_by_iterator();
    test_copy_operations();
    test_move_operations();
    test_value_lifetime();
    test_randomized();

    std::cout << "All HashMap tests passed\n";
}
