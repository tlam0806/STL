// Build from this directory and run:
// g++ -std=c++20 -g -fsanitize=address,undefined -fno-omit-frame-pointer test.cpp -o test && ./test
// Optional: ./test --list or ./test move_assignment_deleter
// Tests run in child processes on macOS/Linux so one crash does not stop the suite.
#include "unique_ptr.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <sys/wait.h>
#include <unistd.h>

namespace {
const char* current_test = "";

void check(bool condition, const char* expression,
           std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::cerr << "[CHECK FAILED] " << current_test << '\n'
                  << "  " << where.file_name() << ':' << where.line() << '\n'
                  << "  Expected true: " << expression << '\n';
        std::cerr.flush();
        std::_Exit(1); // Avoid hiding the first error with a failing destructor.
    }
}
#define CHECK(expression) check((expression), #expression)

template<class A, class B>
void equal(const A& actual, const B& expected, const char* description,
           std::source_location where = std::source_location::current()) {
    if (!(actual == expected)) {
        std::cerr << "[CHECK FAILED] " << current_test << '\n'
                  << "  " << where.file_name() << ':' << where.line() << '\n'
                  << "  " << description << "\n  Expected: " << expected
                  << "\n  Actual:   " << actual << '\n';
        std::cerr.flush();
        std::_Exit(1);
    }
}

struct Tracked {
    static inline int alive = 0;
    static inline int destroyed[8]{};
    int id;
    explicit Tracked(int n) : id(n) { ++alive; }
    Tracked(const Tracked&) = delete;
    ~Tracked() { --alive; ++destroyed[id]; }
};

struct Deleter {
    int* calls = nullptr;
    int expected_id = -1;
    Deleter() noexcept = default;
    Deleter(int& counter, int id) noexcept : calls(&counter), expected_id(id) {}
    Deleter(const Deleter&) noexcept = default;
    Deleter& operator=(const Deleter&) noexcept = default;
    Deleter(Deleter&& other) noexcept
        : calls(std::exchange(other.calls, nullptr)), expected_id(other.expected_id) {}
    Deleter& operator=(Deleter&& other) noexcept {
        calls = std::exchange(other.calls, nullptr);
        expected_id = other.expected_id;
        return *this;
    }
    void operator()(Tracked* p) noexcept {
        CHECK(p != nullptr);
        CHECK(calls != nullptr);
        equal(p->id, expected_id, "pointer must stay paired with its deleter");
        ++*calls;
        delete p;
    }
    friend void swap(Deleter& a, Deleter& b) noexcept {
        using std::swap;
        swap(a.calls, b.calls);
        swap(a.expected_id, b.expected_id);
    }
};
using Owner = unique_ptr<Tracked, Deleter>;

static_assert(!std::is_copy_constructible_v<unique_ptr<int>>);
static_assert(!std::is_copy_assignable_v<unique_ptr<int>>);
static_assert(std::is_nothrow_move_constructible_v<Owner>);
static_assert(std::is_nothrow_move_assignable_v<Owner>);
static_assert(!std::is_convertible_v<unique_ptr<int>, bool>);
static_assert(std::is_same_v<decltype(std::declval<const Owner&>().get()), Tracked*>);
static_assert(std::is_same_v<decltype(std::declval<Owner&>().get_deleter()), Deleter&>);
static_assert(std::is_same_v<decltype(std::declval<const Owner&>().get_deleter()), const Deleter&>);
static_assert(std::is_same_v<decltype(std::declval<Owner&>() = std::declval<Owner&&>()), Owner&>);

void basic_ownership() {
    {
        auto p = ::make_unique<Tracked>(1);
        CHECK(static_cast<bool>(p));
        equal(p.get()->id, 1, "make_unique forwards constructor argument");
        equal(Tracked::alive, 1, "one owned object");
        const auto& cp = p;
        CHECK(cp.get() == p.get());
    }
    equal(Tracked::alive, 0, "destructor releases object");
    equal(Tracked::destroyed[1], 1, "exactly one destruction");
}

void empty_owner() {
    int calls = 0;
    { Owner p(nullptr, Deleter{calls, 1}); CHECK(!p); CHECK(p.get() == nullptr); }
    equal(calls, 0, "empty destructor must not invoke deleter");
}

void forwarding() {
    struct Payload {
        int& reference;
        std::unique_ptr<int> resource;
        Payload(int& ref, std::unique_ptr<int>&& ptr) : reference(ref), resource(std::move(ptr)) {}
    };
    int value = 7;
    auto resource = std::make_unique<int>(42);
    auto p = ::make_unique<Payload>(value, std::move(resource));
    CHECK(!resource);
    CHECK(&p.get()->reference == &value);
    equal(*p.get()->resource, 42, "move-only constructor argument preserved");
}

void throwing_constructor() {
    struct Throws { Throws() { throw std::runtime_error("injected failure"); } };
    bool caught = false;
    try { auto p = ::make_unique<Throws>(); }
    catch (const std::runtime_error&) { caught = true; }
    CHECK(caught);
}

void move_construction() {
    int calls = 0;
    {
        Owner source(new Tracked(1), Deleter{calls, 1});
        Tracked* address = source.get();
        {
            Owner destination(std::move(source));
            CHECK(!source);
            CHECK(destination.get() == address);
            CHECK(destination.get_deleter().calls == &calls);
            equal(calls, 0, "moving must not destroy the object");
        }
        equal(calls, 1, "destination uses transferred deleter");
    }
    equal(calls, 1, "moved-from destructor must not call deleter");
    equal(Tracked::alive, 0, "no live objects after move scopes");
}

void move_assignment_deleter() {
    int old_calls = 0, new_calls = 0;
    {
        Owner destination(new Tracked(1), Deleter{old_calls, 1});
        Owner source(new Tracked(2), Deleter{new_calls, 2});
        Tracked* address = source.get();
        CHECK(&(destination = std::move(source)) == &destination);
        equal(old_calls, 1, "move assignment must use destination's OLD deleter");
        equal(Tracked::destroyed[1], 1, "old object destroyed immediately");
        CHECK(!source);
        CHECK(destination.get() == address);
        CHECK(destination.get_deleter().calls == &new_calls);
        equal(new_calls, 0, "new object not deleted during transfer");
    }
    equal(new_calls, 1, "transferred object deleted by transferred deleter");
    equal(Tracked::alive, 0, "move assignment leaks no objects");
}

void move_empty_source() {
    int calls = 0;
    Owner destination(new Tracked(1), Deleter{calls, 1});
    Owner empty(nullptr, Deleter{calls, 1});
    destination = std::move(empty);
    CHECK(!destination);
    CHECK(!empty);
    equal(calls, 1, "assigning empty owner cleans up destination");
}

void self_move() {
    int calls = 0;
    Owner p(new Tracked(1), Deleter{calls, 1});
    auto* self = &p;
    Tracked* address = p.get();
    p = std::move(*self);
    CHECK(p.get() == address);
    equal(calls, 0, "self move keeps ownership");
}

void swap_ownership() {
    int a_calls = 0, b_calls = 0;
    {
        Owner a(new Tracked(1), Deleter{a_calls, 1});
        Owner b(new Tracked(2), Deleter{b_calls, 2});
        auto* a_address = a.get();
        auto* b_address = b.get();
        a.swap(b);
        CHECK(a.get() == b_address && b.get() == a_address);
        CHECK(a.get_deleter().calls == &b_calls);
        CHECK(b.get_deleter().calls == &a_calls);
        a.swap(a);
        CHECK(a.get() == b_address);
        equal(a_calls + b_calls, 0, "swap must not delete objects");
    }
    equal(a_calls, 1, "first deleter called once");
    equal(b_calls, 1, "second deleter called once");
}

void release_ownership() {
    int calls = 0;
    Tracked* released = nullptr;
    {
        Owner p(new Tracked(1), Deleter{calls, 1});
        auto* address = p.get();
        released = p.release();
        CHECK(released == address);
        CHECK(!p);
        CHECK(p.release() == nullptr);
        equal(calls, 0, "release does not invoke deleter");
    }
    equal(Tracked::alive, 1, "released object survives owner destruction");
    Deleter cleanup(calls, 1);
    cleanup(released);
    equal(calls, 1, "caller can clean up released object");
}

void reset_to_empty() {
    int calls = 0;
    {
        Owner p(new Tracked(1), Deleter{calls, 1});
        p.reset();
        CHECK(p.get() == nullptr);
        CHECK(!p);
        equal(calls, 1, "reset deletes old object");
        p.reset();
        equal(calls, 1, "reset on empty owner does nothing");
    }
    equal(calls, 1, "no second deletion after reset");
}

void reset_replacement() {
    int calls = 0;
    {
        Owner p(new Tracked(1), Deleter{calls, 1});
        auto* replacement = new Tracked(1);
        p.reset(replacement);
        CHECK(p.get() == replacement);
        equal(calls, 1, "replacement destroys old object");
        equal(Tracked::alive, 1, "only replacement remains alive");
    }
    equal(calls, 2, "replacement destroyed at scope exit");
}

void reset_empty_owner() {
    int calls = 0;
    {
        Owner p(nullptr, Deleter{calls, 1});
        auto* replacement = new Tracked(1);
        p.reset(replacement);
        CHECK(p.get() == replacement);
        equal(calls, 0, "no deleter call for previously null pointer");
    }
    equal(calls, 1, "new ownership is released");
}

void deleter_access() {
    int original_calls = 0, redirected_calls = 0;
    {
        Owner p(new Tracked(1), Deleter{original_calls, 1});
        const Owner& view = p;
        CHECK(&view.get_deleter() == &p.get_deleter());
        p.get_deleter().calls = &redirected_calls;
    }
    equal(original_calls, 0, "accessor returns a reference, not a copy");
    equal(redirected_calls, 1, "modified deleter state is used");
}

struct Test { const char* name; void (*run)(); };
bool run_test(const Test& test) {
    current_test = test.name;
    std::cout << "[RUN] " << test.name << std::endl;
    std::cerr.flush();
    pid_t child = fork();
    if (child < 0) { std::cerr << "fork failed\n"; return false; }
    if (child == 0) {
        alarm(30);
        try {
            test.run();
            equal(Tracked::alive, 0, "no objects leaked by test");
            std::cout.flush(); std::cerr.flush();
            std::_Exit(0);
        } catch (const std::exception& e) {
            std::cerr << "[UNEXPECTED EXCEPTION] " << test.name << ": " << e.what() << '\n';
        } catch (...) { std::cerr << "[UNEXPECTED EXCEPTION] " << test.name << '\n'; }
        std::cerr.flush(); std::_Exit(1);
    }
    int status = 0;
    pid_t result;
    do { result = waitpid(child, &status, 0); } while (result < 0 && errno == EINTR);
    bool passed = result == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name;
    if (result == child && WIFSIGNALED(status)) std::cout << " (signal " << WTERMSIG(status) << ')';
    std::cout << std::endl;
    return passed;
}
} // namespace

int main(int argc, char** argv) {
    const Test tests[] = {
        {"basic_ownership", basic_ownership}, {"empty_owner", empty_owner},
        {"forwarding", forwarding}, {"throwing_constructor", throwing_constructor},
        {"move_construction", move_construction}, {"move_assignment_deleter", move_assignment_deleter},
        {"move_empty_source", move_empty_source}, {"self_move", self_move},
        {"swap_ownership", swap_ownership}, {"release_ownership", release_ownership},
        {"reset_to_empty", reset_to_empty}, {"reset_replacement", reset_replacement},
        {"reset_empty_owner", reset_empty_owner}, {"deleter_access", deleter_access},
    };
    if (argc > 2) { std::cerr << "Usage: " << argv[0] << " [--list|test_name]\n"; return 2; }
    if (argc == 2 && std::string_view(argv[1]) == "--list") {
        for (const auto& t : tests) std::cout << t.name << '\n';
        return 0;
    }
    int count = 0, failures = 0;
    for (const auto& t : tests) {
        if (argc == 2 && std::string_view(argv[1]) != t.name) continue;
        ++count;
        failures += !run_test(t);
    }
    if (!count) { std::cerr << "Unknown test; use --list\n"; return 2; }
    std::cout << "Summary: " << count - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
