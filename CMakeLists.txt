cmake_minimum_required(VERSION 4.1)
project(GraphProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)



add_executable(GraphProject GTest.cpp
        Graph.hpp
        GTest.cpp)

target_link_libraries(GraphProject gtest_main)

include(GoogleTest)
gtest_discover_tests(GraphProject)
