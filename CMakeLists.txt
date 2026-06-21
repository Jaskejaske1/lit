# lit — substrate-driven lighting engine
#
# Architecture: library first.
#   - substrate (STATIC library): the engine core. Holds Engine + node types.
#     This will grow into the real substrate (node graph, math, fixtures, viz).
#   - lit (EXECUTABLE): the current prototype binary. Will be replaced by
#     Builder / Operation / Visualizer in later phases.
#
# All dependencies fetched via CMake FetchContent (no vendored deps, no init script).
# Build system: CMake 3.20+.

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
# GL3W's CMakeLists creates an INTERFACE library "gl3w" (include dirs only)
# and generates gl3w.c at configure time. We just compile that generated source.
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

# ImGui doesn't ship a CMakeLists that builds the library; build the parts we use.
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
# substrate library (architecture: library first, three thin binaries link it)
# ============================================================================
add_library(substrate STATIC
    src/Engine.cpp
    src/Nodes/ComputeNode.cpp
    src/Nodes/VisualizerNode.cpp
)
target_include_directories(substrate PUBLIC src)
target_link_libraries(substrate PUBLIC imgui gl3w_loader SDL3::SDL3)

# ============================================================================
# Executable: lit
# ============================================================================
add_executable(lit src/main.cpp)
target_link_libraries(lit PRIVATE substrate)

# ============================================================================
# Compiler flags
# ============================================================================
if(MSVC)
    target_compile_options(substrate PRIVATE /W3 /utf-8 /EHsc)
    target_compile_options(lit       PRIVATE /W3 /utf-8 /EHsc)
    if(LIT_WARNINGS_AS_ERRORS)
        target_compile_options(substrate PRIVATE /WX)
        target_compile_options(lit       PRIVATE /WX)
    endif()
else()
    target_compile_options(substrate PRIVATE -Wall -Wextra)
    target_compile_options(lit       PRIVATE -Wall -Wextra)
    if(LIT_WARNINGS_AS_ERRORS)
        target_compile_options(substrate PRIVATE -Werror)
        target_compile_options(lit       PRIVATE -Werror)
    endif()
endif()

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
message(STATUS "")
