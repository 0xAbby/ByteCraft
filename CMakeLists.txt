cmake_minimum_required(VERSION 3.20)
project(bytecraft CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bytecraft_core
  src/bytecode.cpp
  src/asm.cpp
  src/vm.cpp
)

target_include_directories(bytecraft_core PUBLIC include)

add_executable(bytecraft src/main.cpp)
target_link_libraries(bytecraft PRIVATE bytecraft_core)


include(FetchContent)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(bytecraft_tests
  tests/test_vm_registers.cpp
)

target_link_libraries(bytecraft_tests
  PRIVATE
    bytecraft_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(bytecraft_tests)