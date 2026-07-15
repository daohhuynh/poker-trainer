#!/bin/zsh
# Builds the native reconciliation benchmark by compiling the REAL shipping source
# files (no reimplementation) at -O2 and linking with the harness. No LTO: the
# cross-translation-unit call boundary between the harness and the reconcile
# functions is preserved exactly as it exists in the app (bridge and backbone are
# separate TUs / static libs there too), so the compiler cannot fold or elide the
# per-frame calls.
set -e
cd "$(dirname "$0")/.."   # repo root

CXX=${CXX:-c++}
FLAGS=(-std=c++23 -O2 -DNDEBUG -Wall -Wextra -I src -I bench_results)

$CXX "${FLAGS[@]}" \
    bench_results/reconcile_bench.cpp \
    src/bridge/focus_registry.cpp \
    src/bridge/focus_reconcile.cpp \
    src/backbone/focus_manager.cpp \
    -o bench_results/reconcile_bench

echo "built bench_results/reconcile_bench"
