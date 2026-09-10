# LFU cache checks

From the repository root:

```sh
sh LFUCache/run_tests.sh
```

The script builds into a temporary directory, runs all three executables, and returns nonzero if any compilation or execution fails. It defaults to `clang++`; set `CXX` to select another compiler. Runtime tests require macOS/Linux for child-process isolation. AddressSanitizer and UndefinedBehaviorSanitizer are enabled, with recovery disabled. No implementation files are modified.

- `header_test.cpp`: includes the header first and twice, then instantiates basic operations. This checks the current compiler's header integration; transitive includes can still hide missing direct dependencies on other standard libraries.
- `test.cpp`: existing eviction, tie-breaking, frequency updates, capacity, construction-failure and 30,000-operation differential checks. The script enables the optional copy-assignment check; explicitly deleted copy operations are accepted and reported as such, not exercised as supported behavior.
- `extended_test.cpp`: seven additional groups covering move construction/source destruction/reuse, populated move assignment across different capacities, swaps/self-move/zero capacity, full-cache insertion failure, throwing return-value copy, throwing update assignment, and string-key ownership with tracked value lifetimes.

The move tests reflect the current documented-in-code behavior: move construction leaves a zero-capacity source, while swap-based move assignment transfers the destination's old state into the source. A future change to that policy may require adjusting these expectations.

Exception checks require a usable cache and correctly managed objects, not complete rollback: a failed insertion into a full cache can lose the previously evicted entry, and failed get/update operations can already have increased frequency. These behaviors should be described if this is used as an application sample.

Validation: all three executables passed with Apple Clang on macOS using the script. Finite tests do not prove correctness for every type or workload. Allocation failures, throwing hash/equality functions, frequency-counter overflow, concurrency, and performance comparisons are not covered. Child-process `_Exit` does not provide a process-exit leak audit; explicit value counters check the exercised value lifetimes.
