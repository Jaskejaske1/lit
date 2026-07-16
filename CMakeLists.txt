# lit — substrate-driven lighting engine
#
# Architecture: library first (substrate + thin binaries).
# Build system: CMake 3.20+ with FetchContent for all deps.
# Source files: src/ (substrate code lives here when added in Phase 1+).

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

# Static libs everywhere (no DLL hell)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries (default: OFF = static)")

# Output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Options
# ============================================================================
option(LIT_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# ============================================================================
# Dependencies (all via FetchContent — single source of truth in this file)
# ============================================================================

# --- SDL3 (built from source, statically linked) ---
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# --- GL3W ---
FetchContent_Declare(
    gl3w
    GIT_REPOSITORY https://github.com/skaslev/gl3w.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(gl3w)

add_library(gl3w_loader STATIC ${gl3w_BINARY_DIR}/src/gl3w.c)
target_link_libraries(gl3w_loader PUBLIC gl3w)

# --- ImGui ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GL3W)
target_link_libraries(imgui PUBLIC gl3w_loader SDL3::SDL3)

# ============================================================================
# Substrate — the core library (Phase 1+)
# ============================================================================
# Header-only for v0: type system, sockets, nodes, graph core, registry.
# All other targets (tests, executables) link against `substrate` and
# include <substrate/substrate.h>.

add_library(substrate STATIC
    src/substrate/value.h
    src/substrate/socket.h
    src/substrate/node.h
    src/substrate/graph.h
    src/substrate/builtins.h
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
# lit_view — substrate dev console (Phase 1.a, the seed of the Builder binary)
# ============================================================================
# Minimum host shell: window + OpenGL 4.6 + ImGui + a debug panel that
# inspects the substrate's data model and ticks a real Graph.

add_executable(lit_view src/view/main.cpp)
target_link_libraries(lit_view PRIVATE substrate imgui)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "lit ${PROJECT_VERSION} — substrate-driven lighting engine")
message(STATUS "  C++ compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Generator:       ${CMAKE_GENERATOR}")
message(STATUS "  Platform:        ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "  Output (exec):   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Output (libs):   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "  Substrate:       src/substrate/ (header-only static lib)")
message(STATUS "  Tests:           tests/test_substrate")
message(STATUS "  lit_view:        src/view/  (substrate dev console)")
message(STATUS "")
