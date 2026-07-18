cmake_minimum_required(VERSION 3.20)
project(lit
    VERSION 0.1.0
    DESCRIPTION "Substrate-driven lighting engine"
    LANGUAGES C CXX
)

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
