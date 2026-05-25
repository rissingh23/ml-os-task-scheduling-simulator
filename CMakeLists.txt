cmake_minimum_required(VERSION 3.20)

project(ml_os_task_scheduler_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(
  scheduler_core
  src/benchmark.cpp
  src/feature_export.cpp
  src/ml_model.cpp
  src/schedulers/fifo_scheduler.cpp
  src/schedulers/ml_scheduler.cpp
  src/schedulers/mlfq_scheduler.cpp
  src/simulator.cpp
  src/trace_loader.cpp
  src/types.cpp
  src/workload.cpp
)

target_include_directories(scheduler_core PUBLIC include)

target_compile_options(
  scheduler_core
  PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic>
)

add_executable(scheduler_sim src/main.cpp)
target_link_libraries(scheduler_sim PRIVATE scheduler_core)

enable_testing()

add_executable(scheduler_tests tests/test_scheduler.cpp)
target_link_libraries(scheduler_tests PRIVATE scheduler_core)

add_test(NAME scheduler_tests COMMAND scheduler_tests)
