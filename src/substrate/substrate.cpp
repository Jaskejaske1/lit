// Substrate library translation unit.
//
// Substrate is header-only; this .cpp exists so that:
//   1. CMake has a C++ file to anchor the static lib's linker language.
//   2. The compiler parses every substrate header at build time, so header
//      errors surface here rather than only in downstream TUs.

#include <substrate/substrate.h>
