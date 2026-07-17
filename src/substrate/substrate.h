#pragma once

// lit — substrate umbrella header
//
// Single include for everything in the substrate library. Other targets
// (tests, executables, future modules) link against `substrate` and
// `#include <substrate/substrate.h>` to pull all types in.

#include "value.h"
#include "fixture.h"
#include "socket.h"
#include "node.h"
#include "graph.h"
#include "builtins.h"
