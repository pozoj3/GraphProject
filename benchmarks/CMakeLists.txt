add_executable(run_benchmarks
    bench_main.cpp
)

target_include_directories(run_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(run_benchmarks PRIVATE benchmark::benchmark)