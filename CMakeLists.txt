cmake_minimum_required(VERSION 3.20)
project(TogyzkumalakEngine VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Strict ISO C++

# Interprocedural Optimization (LTO)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Global Optimization Flags
if(NOT EMSCRIPTEN)
    # Native execution bounds
    add_compile_options(
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>
        $<$<CXX_COMPILER_ID:GNU>:-Wall>
        $<$<CXX_COMPILER_ID:GNU>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU>:-Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wall>
        $<$<CXX_COMPILER_ID:Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wpedantic>
    )
else()
    # WebAssembly Emscripten optimizations
    add_compile_options(-O3 -s MALLOC=emmalloc)
    add_link_options(-O3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1)
endif()

# Core Library Definition
add_library(togyz_core STATIC
    src/togyzkumalak_rules.cpp
    src/position_hash.cpp
    src/minimax_engine.cpp
    src/dag_search.cpp
    src/evaluation.cpp
)
target_include_directories(togyz_core PUBLIC include)

# Executables (Only build if NOT targeting WASM)
if(NOT EMSCRIPTEN)
    # CLI Engine
    add_executable(tg_engine tools/cli_engine/togyzkumalak_engine.cpp)
    target_link_libraries(tg_engine PRIVATE togyz_core)

    # Billion Game Simulator
    add_executable(tg_billion_sim tools/billion_game_sim/generate_billion_games.cpp)
    target_link_libraries(tg_billion_sim PRIVATE togyz_core)

    # Shortest Game Solver
    add_executable(tg_shortest_solver tools/shortest_game_solver/find_shortest_game.cpp)
    target_link_libraries(tg_shortest_solver PRIVATE togyz_core)

    # Perft Testing Suite
    add_executable(perft_test tests/perft.cpp)
    target_link_libraries(perft_test PRIVATE togyz_core)
endif()
