cmake_minimum_required(VERSION 3.20)
project(lit
    VERSION 0.1.0
    DESCRIPTION "Substrate-driven lighting engine"
    LANGUAGES C CXX
)

include(FetchContent)

# ============================================================================
# Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries (default: OFF = static)")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(LIT_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# ============================================================================
# Substrate — core library (header-only style with a small .cpp anchor)
# ============================================================================
add_library(substrate STATIC
    src/substrate/value.h
    src/substrate/socket.h
    src/substrate/node.h
    src/substrate/graph.h
    src/substrate/builtins.h
    src/substrate/demo_patch.h
    src/substrate/substrate.h
    src/substrate/substrate.cpp
)
target_include_directories(substrate PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ============================================================================
# Tests
# ============================================================================
add_executable(test_substrate tests/test_substrate.cpp)
target_link_libraries(test_substrate PRIVATE substrate)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "lit ${PROJECT_VERSION} — substrate-driven lighting engine")
message(STATUS "  C++ compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Generator:       ${CMAKE_GENERATOR}")
message(STATUS "  Substrate:       src/substrate/ (static lib)")
message(STATUS "  Tests:           test_substrate")
message(STATUS "")

# ============================================================================
# Experiment dependencies (SDL3 + GL3W)
# ============================================================================

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# GL3W
FetchContent_Declare(
    gl3w
    GIT_REPOSITORY https://github.com/skaslev/gl3w.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(gl3w)

add_library(gl3w_loader STATIC ${gl3w_BINARY_DIR}/src/gl3w.c)
target_link_libraries(gl3w_loader PUBLIC gl3w)

# ============================================================================
# Experiment 01 – Compute shader sine field
# ============================================================================
add_executable(experiment_01
    src/experiments/01_compute_sine_field/main.cpp
)
target_link_libraries(experiment_01 PRIVATE SDL3::SDL3 gl3w_loader)

# ============================================================================
# Experiment 02 – Fixture sampling from compute shader field
# ============================================================================
add_executable(experiment_02
    src/experiments/02_fixture_sampling/main.cpp
)
target_link_libraries(experiment_02 PRIVATE SDL3::SDL3 gl3w_loader)

# ============================================================================
# Experiment 03 – Multi‑Channel Fixture Output
# ============================================================================
add_executable(experiment_03
    src/experiments/03_multi_channel_output/main.cpp
)
target_link_libraries(experiment_03 PRIVATE SDL3::SDL3 gl3w_loader)

# ============================================================================
# Experiment 04 – Visual Fixture Overlay
# ============================================================================
add_executable(experiment_04
    src/experiments/04_visual_overlay/main.cpp
)
target_link_libraries(experiment_04 PRIVATE SDL3::SDL3 gl3w_loader)

# ============================================================================
# Experiment 05 – Connected Field Sampling
# ============================================================================
add_executable(experiment_05
    src/experiments/05_connected_sampling/main.cpp
)
target_link_libraries(experiment_05 PRIVATE SDL3::SDL3 gl3w_loader)
