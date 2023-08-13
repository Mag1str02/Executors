cmake_minimum_required(VERSION 3.20)
set(CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

project(Executors)

include(gmock.cmake)
include(benchmark.cmake)

add_library(executors 
  executors.cpp
)

add_executable(test_executors
  test_executors.cpp
  test_future.cpp
  executors.cpp
  contrib/gmock_main.cc
)
target_link_libraries(test_executors PUBLIC gmock)

target_compile_options(test_executors PUBLIC -fsanitize=thread)
target_link_options(test_executors PUBLIC -fsanitize=thread)

add_executable(bench_executors
  run.cpp
  executors.cpp
)

target_link_libraries(bench_executors benchmark)