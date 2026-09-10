# Vector

An educational C++20 vector-like container. This is a supported-subset implementation, not a claim of full `std::vector` conformance.

Run the tests from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target vector_test
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build -R '^vector_test$' --output-on-failure
```

Debug builds with Clang/GCC enable AddressSanitizer and UndefinedBehaviorSanitizer. Tests use explicit checks, so they also remain active in Release builds.

Coverage:

- Element access, bounds errors, iterators, capacity operations and empty ranges.
- Independent copies, moves, swaps, self-assignment and moved-from reuse.
- Aliasing arguments during append, insertion and resize.
- Move-only values.
- 30,000 seeded operations compared with `std::vector`, checking contents and returned iterators without assuming its capacity-growth policy.
- Object lifetimes and injected failures during construction, copying, resizing, reallocation, shrinking, append and middle insertion.
- Over-aligned elements and excessive-capacity rejection.

Storage uses matching aligned allocation/deallocation for over-aligned types. During middle insertion, the newly constructed tail is counted before shifting assignments: if assignment throws, values and size may change, but all live elements remain owned and destructible. These tests check that basic lifetime guarantee, not strong rollback for insertion.

Limitations: unchecked access, `front`, `back`, and `pop_back` require valid/nonempty input as appropriate; positional modifiers require valid iterators. Allocator customization, complete standard iterator/API conformance, exhaustive failure injection, and performance superiority are not covered. Throwing move-only relocation is not covered by this suite. Do not infer correctness for every type or workload from the finite tests.
