cmake_minimum_required(VERSION 3.20)
project(opticore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(OPTICORE_BUILD_TESTS "Build test executables" ON)
option(OPTICORE_WERROR "Treat compiler warnings as errors" OFF)

add_library(opticore_core STATIC
  core/src/types.cpp
  core/src/scenario.cpp
  core/src/solution.cpp
  core/src/route_planner.cpp
  core/src/objective.cpp
  core/src/validator.cpp
  simulation/city_model.cpp
  solvers/cpu/chronos.cpp
)
target_include_directories(opticore_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/include
  ${CMAKE_CURRENT_SOURCE_DIR}/simulation
  ${CMAKE_CURRENT_SOURCE_DIR}/solvers/cpu
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(opticore_core PRIVATE -Wall -Wextra -Wpedantic)
  if(OPTICORE_WERROR)
    target_compile_options(opticore_core PRIVATE -Werror)
  endif()
endif()

add_executable(opticore tools/opticore-cli.cpp)
target_link_libraries(opticore PRIVATE opticore_core)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(opticore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(OPTICORE_BUILD_TESTS)
  enable_testing()
  add_executable(oc_tests_core tests/test_core.cpp)
  target_link_libraries(oc_tests_core PRIVATE opticore_core)
  add_test(NAME core_tests COMMAND oc_tests_core)

  add_executable(oc_tests_solver tests/test_solver.cpp)
  target_link_libraries(oc_tests_solver PRIVATE opticore_core)
  add_test(NAME solver_tests COMMAND oc_tests_solver)
endif()
