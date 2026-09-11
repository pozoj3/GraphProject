add_executable(run_all_tests
    test.cpp
    test_naive.cpp
    test_csr.cpp
    test_cblist.cpp
    test_all.cpp
)

target_include_directories(run_all_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(run_all_tests PRIVATE GTest::gtest GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(run_all_tests)