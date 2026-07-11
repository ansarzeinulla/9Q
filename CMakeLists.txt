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
    src/dag_v1_search.cpp
    src/dag_v2_search.cpp
    src/evaluation.cpp
    src/console_render.cpp
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

    # Coverage Option
    option(ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
    if(ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            add_compile_options(--coverage -O0 -g)
            add_link_options(--coverage)
        endif()
    endif()

    # Google Test Integration
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # Unit Tests
    add_executable(test_zobrist_hashing tests/test_zobrist_hashing.cpp)
    target_link_libraries(test_zobrist_hashing PRIVATE togyz_core gtest_main)
    add_test(NAME test_zobrist_hashing COMMAND test_zobrist_hashing)

    add_executable(test_tt_eviction tests/test_tt_eviction.cpp)
    target_link_libraries(test_tt_eviction PRIVATE togyz_core gtest_main)
    add_test(NAME test_tt_eviction COMMAND test_tt_eviction)

    add_executable(test_evaluation tests/test_evaluation.cpp)
    target_link_libraries(test_evaluation PRIVATE togyz_core gtest_main)
    add_test(NAME test_evaluation COMMAND test_evaluation)

    add_executable(test_eval_params tests/test_eval_params.cpp)
    target_link_libraries(test_eval_params PRIVATE togyz_core gtest_main)
    add_test(NAME test_eval_params COMMAND test_eval_params)

    add_executable(test_dag_v2_tables tests/test_dag_v2_tables.cpp)
    target_link_libraries(test_dag_v2_tables PRIVATE togyz_core gtest_main)
    add_test(NAME test_dag_v2_tables COMMAND test_dag_v2_tables)

    add_executable(test_dag_v2_aspiration tests/test_dag_v2_aspiration.cpp)
    target_link_libraries(test_dag_v2_aspiration PRIVATE togyz_core gtest_main)
    add_test(NAME test_dag_v2_aspiration COMMAND test_dag_v2_aspiration)

    add_executable(test_dag_v2_equivalence tests/test_dag_v2_equivalence.cpp)
    target_link_libraries(test_dag_v2_equivalence PRIVATE togyz_core gtest_main)
    add_test(NAME test_dag_v2_equivalence COMMAND test_dag_v2_equivalence)
endif()

