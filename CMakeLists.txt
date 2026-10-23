cmake_minimum_required(VERSION 3.16)
project(vlogsim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(VLOGSIM_ENABLE_SANITIZERS "Build with ASan/UBSan" OFF)

add_library(vlogsim_core
    src/token.cpp
    src/lexer.cpp
    src/parser.cpp
    src/elaborator.cpp
    src/comb_evaluator.cpp
    src/seq_evaluator.cpp
    src/vcd_writer.cpp
    src/flattener.cpp
    src/four_state_evaluator.cpp
)
target_include_directories(vlogsim_core PUBLIC include)
target_compile_options(vlogsim_core PRIVATE -Wall -Wextra)

if(VLOGSIM_ENABLE_SANITIZERS)
    target_compile_options(vlogsim_core PUBLIC -fsanitize=address,undefined -g)
    target_link_options(vlogsim_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(vlogsim_cli src/main.cpp)
target_link_libraries(vlogsim_cli PRIVATE vlogsim_core)

enable_testing()
add_subdirectory(tests)
