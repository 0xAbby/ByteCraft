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