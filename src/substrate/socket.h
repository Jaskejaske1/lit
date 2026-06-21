#pragma once

#include <optional>
#include <string>
#include <utility>

#include "value.h"

// lit — substrate sockets
//
// A Socket is a typed data port on a Node. Inputs are parameters with default
// values; outputs are produced values. Per data-model.txt:62-75 and
// ideas.txt:217-223, "a Socket IS the parameter" — one concept, two roles
// based on direction.
//
// Sockets come in two flavors:
//   SocketSpec  — schema entry, lives in NodeType. Declares what a port IS.
//   Socket      — per-instance port, lives in Node. Has the live `current` value.
//
// Connection is a separate object on the Graph (not a field on Socket).
// The engine writes the connection source's value into input.current before
// calling Evaluate. See node.h for the rule.

namespace substrate {

enum class SocketDirection : uint8_t { Input, Output };

// Schema entry — what a Type declares about a port.
// Lives in NodeType.{inputs, outputs}. Drives make_node().
struct SocketSpec {
    std::string                          name;            // human-readable label
    ValueType                            type = ValueType::Scalar;
    SocketValue                          default_value;   // for inputs; ignored for outputs
    std::optional<std::pair<float, float>> range;         // optional UI slider bounds
};

// Per-instance socket. `current` is the live value, type matches the spec.
// Lifecycle: make_node() copies spec into socket with current = default_value.
// Engine/Graph overwrites current each tick (from connection source if connected,
// else leaves default_value).
struct Socket {
    std::string                          name;
    ValueType                            type = ValueType::Scalar;
    SocketDirection                      direction = SocketDirection::Input;
    SocketValue                          default_value;   // constant literal; ignored if connected
    SocketValue                          current;         // live value
    std::optional<std::pair<float, float>> range;
};

}  // namespace substrate
