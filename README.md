# GraphProject

A header-only C++20 library of graph storage structures behind a single,
statically dispatched interface, together with a reproducible Google Benchmark
suite, a GoogleTest test suite, and a Jupyter notebook that visualizes the
results.

The project studies one question: how much does the in-memory layout of a graph
matter for real performance? To keep the comparison fair, all six storage
structures share one implementation of BFS, DFS and Dijkstra (through the CRTP
base class), so the layout is the only thing that changes between them.

## The structures

All of them live in include/graphs and satisfy the same compile-time interface.

- NaiveGraph — a classic adjacency list (a hash map of vertex to a vector of
  neighbours). Constant-time insertion, linear-in-degree lookup. It is the
  readability and behaviour reference.
- RawGraph — a single flat edge list. Cheap to append to and to scan in bulk,
  but every point query is a linear scan of all edges, so it is used only as a
  baseline.
- CSRGraph — Compressed Sparse Row with an explicit finalize() step. Edges are
  staged, then sorted and de-duplicated; after that, lookups are a binary
  search.
- StaticCSRGraph — an immutable CSR built once. The most compact and the fastest
  for read-only work; any attempt to mutate it throws.
- DynamicCSRGraph — a CSR that rebuilds itself on every insertion. Kept as a
  cautionary baseline, because building it edge by edge is quadratic.
- CBListGraph — a chunked adjacency list whose 64-byte-aligned chunks hold 14
  edges each (exactly 192 bytes with double weights).
- GCCGraph — the storage layer of GastCoCo: small per-vertex chunks that are
  promoted to a per-vertex B+ tree, all threaded onto one Global Traversal
  Chain.

Runtime-polymorphic (virtual) versions live in include/vgraphs and are used only
to measure the cost of virtual dispatch against the CRTP approach.

GCCGraph reimplements the storage layer of GastCoCo (arXiv 2312.14396, PVLDB
17(13), 2024); the coroutine-based prefetch engine from that paper is out of
scope here.

## Requirements

- A C++20 compiler: GCC 13 or newer, or Clang 16 or newer.
- CMake 3.20 or newer.
- Internet access on the first configure. GoogleTest and Google Benchmark are
  downloaded automatically by CMake, so nothing has to be installed by hand.

## Building

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)

Always build in Release mode for benchmarking. A debug build has no
optimizations and its numbers are meaningless.

## Running the tests

Run the whole suite through ctest:

    ctest --test-dir build --output-on-failure

Or run the test binary directly, optionally with a filter:

    ./build/tests/run_all_tests
    ./build/tests/run_all_tests --gtest_filter=*Dijkstra*

## Running the benchmarks

Write the results to the file the notebook reads:

    ./build/benchmarks/run_benchmarks --benchmark_out=results/results_advanced.json --benchmark_out_format=json

A quicker pass, with a shorter timing budget per case:

    ./build/benchmarks/run_benchmarks --benchmark_min_time=0.05s --benchmark_out=results/results_advanced.json --benchmark_out_format=json

Steadier numbers, by repeating and reporting aggregates:

    ./build/benchmarks/run_benchmarks --benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out=results/results_advanced.json --benchmark_out_format=json

Only some families of benchmarks:

    ./build/benchmarks/run_benchmarks --benchmark_filter=Density

## Looking at the results

The notebook results/benchmark_results.ipynb loads the results JSON and plots
every family of benchmark: insertion, lookup, traversal, full scan, BFS,
Dijkstra, the density sweep, the topologies, pointer-chasing latency, the memory
footprint, and template-versus-virtual dispatch.

Set up a small environment and open it:

    python3 -m venv .venv
    .venv/bin/pip install pandas matplotlib ipykernel

Then open results/benchmark_results.ipynb, choose the .venv kernel, and run all
cells. If you set RUN_BENCHMARKS to True in the second cell, the notebook runs
the compiled binary itself before plotting.

## Continuous integration

The workflow in .github/workflows/tests.yml builds the project and runs the test
suite with both GCC and Clang on every push and pull request.

## Repository layout

- include/graphs — the six storage structures plus the shared base class, the interface concept, and the graph generators.
- include/vgraphs — the virtual variants used in the polymorphism benchmark, this is not the focus of the projct but just a comparsion.
- tests — the GoogleTest suites, built into run_all_tests.
- benchmarks — the Google Benchmark suite, built into run_benchmarks.
- results — the results JSON and the analysis notebook.

## License

GNU General Public License v3. See the LICENSE file.