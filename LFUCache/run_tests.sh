#!/bin/sh
# Run all checks even when one compile or runtime check fails.
set -u
cd "$(dirname "$0")" || exit 1
lfu_build_dir=$(mktemp -d "${TMPDIR:-/tmp}/lfu-checks.XXXXXX") || exit 1
trap 'rm -rf "$lfu_build_dir"' EXIT HUP INT TERM
lfu_compiler=${CXX:-clang++}
lfu_failed=0
for lfu_source in header_test.cpp test.cpp extended_test.cpp; do
    printf '\nChecking %s\n' "$lfu_source"
    if "$lfu_compiler" -std=c++20 -O1 -g -Wall -Wextra -Wpedantic \
        -fsanitize=address,undefined -fno-sanitize-recover=all \
        -DLFU_TEST_COPY_ASSIGNMENT "$lfu_source" -o "$lfu_build_dir/test"; then
        "$lfu_build_dir/test" || lfu_failed=1
    else
        lfu_failed=1
    fi
done
exit "$lfu_failed"
