# High-Performance C++ Graph & Memory Benchmark Suite

A C++ implementation of three distinct graph data structure architectures with an emphasis on hardware-level memory optimization (cache-line alignment), custom block allocation, and throughput.

The project evaluates and benchmarks the following implementations:
- NaiveGraph: A conventional adjacency list based on std::unordered_map and std::vector.
- CSRGraph: A static Compressed Sparse Row representation optimized for cache locality and binary search, requiring finalize() before lookups.
- CBListGraph: A hybrid chunked linked-list architecture utilizing 14-element blocks (EdgeChunk) aligned to 64-byte boundaries (alignas(64), exactly 192 bytes) powered by a custom block allocator (ChunkArena).

Inspired by research on bridging static and dynamic graph systems:
Paper: Bridging the Gap between Dynamic and Static Graph Processing (VLDB)
Reference: https://www.vldb.org/pvldb/vol17/p4827-li.pdf


## Prerequisites

- Modern C++ compiler with C++20 / C++23 support (GCC 13+ or Clang 16+)
- CMake (version 3.20 or newer)
- Installed libraries: Google Test (GTest) and Google Benchmark


## Building the Project

Navigate to the project root directory and run:

    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . -j$(nproc)

Windows build (PowerShell/CMD):
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . --config Release


## Running Tests (Google Test)

Execute all unit and parameterized integration tests using ctest inside the build directory:

    ctest --output-on-failure

Or run individual test binaries directly:

    ./test_naive
    ./test_csr
    ./test_cblist
    ./test_all

Run specific tests using GTest filters:

    ./test_all --gtest_filter="*Dijkstra*"


## Running Benchmarks (Google Benchmark)

Run the full benchmark suite directly:

    ./run_benchmarks

Filter specific benchmark suites:

    # Benchmark only initial edge insertions:
    ./run_benchmarks --benchmark_filter="BM_EdgeInsertion"

    # Benchmark Breadth-First Search (BFS):
    ./run_benchmarks --benchmark_filter=".*BFS.*"

    # Compare performance of direct vs. external storage for complex/heavy types:
    ./run_benchmarks --benchmark_filter=".*BigClass.*"

    # Benchmark specific graph topologies (Sparse, Dense, R-MAT, 2D Grid):
    ./run_benchmarks --benchmark_filter=".*Topology.*"
    
    # Compare template (static) vs. virtual (polymorphic) graph performance:
    ./run_benchmarks --benchmark_filter=".*Polymorphism.*"

Export benchmark results to JSON:

    ./run_benchmarks --benchmark_out=benchmark_results.json --benchmark_out_format=json
    