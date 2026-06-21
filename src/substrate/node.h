#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "socket.h"
#include "value.h"

// lit — substrate Node + NodeType + registry
//
// Architecture (data-model.txt:17-60, ideas.txt:205-215):
//   - Flat registry of node types. No class inheritance on Node.
//   - NodeType IS a struct: name + schemas + math function pointer.
//   - Node is a per-instance struct: id + borrowed type pointer + per-instance
//     state + per-instance sockets.
//   - Mirrors Max/MSP's maxclass: a struct with a function pointer for the math.
//
// Evaluate signature:
//   void evaluate(Node& self, float dt, float elapsed, bool init_pass)
//     - dt         = seconds since last tick
//     - elapsed    = total seconds since graph start
//     - init_pass  = true during the cold-start init pass (data-model:122):
//                    "Time does NOT advance. Counters do NOT increment."
//                    Nodes that integrate time check this flag and skip.
//
// Connection model (data-model.txt:159-171, ideas.txt:225-229):
//   Connection is a separate object on the Graph, NOT a field on Socket.
//   Before calling Evaluate on a node, the engine writes connection source
//   values into input.current. The Node's evaluate() can then read inputs[]
//   as if they were already populated. Nodes never touch Connection objects.

namespace substrate {

using NodeId = uint64_t;

// What a Type declares about a state key. Lives in NodeType.state_schema.
// Drives make_node(), which copies the default_value into Node.state at
// construction (data-model:120 — type-level cold-start values).
struct StateKeySpec {
    std::string   name;
    ValueType     type;
    SocketValue   default_value;
};

struct Node;  // forward — EvaluateFn references it

// The math signature. init_pass = true means "produce initial outputs from
// current state + initial inputs; do not advance time or increment counters."
using EvaluateFn = void (*)(Node& self, float dt, float elapsed, bool init_pass);

// Flat registry entry. This IS the type system — no class hierarchy.
// (data-model.txt:38-60 — "Flat list of node types. No inheritance.")
struct NodeType {
    std::string                  name;            // unique identifier, e.g. "Phase"
    std::string                  display_name;    // for UI
    std::string                  category;        // for UI grouping
    std::vector<SocketSpec>      inputs;          // schema; copied to Node at make_node()
    std::vector<SocketSpec>      outputs;         // schema; copied to Node at make_node()
    std::vector<StateKeySpec>    state_schema;    // default state keys + cold-start values
    EvaluateFn                   evaluate;        // the math
};

// Per-instance. Type is borrowed from the registry. All instance-specific
// state lives here: position, name, per-instance socket current values, state dict.
struct Node {
    NodeId                                            id = 0;
    const NodeType*                                   type = nullptr;  // borrowed
    std::string                                       name;           // instance label (identity)
    Vec2                                              position{0, 0}; // UI-only, not math
    std::vector<Socket>                               inputs;         // per-instance, built from type schema
    std::vector<Socket>                               outputs;        // per-instance, built from type schema
    std::unordered_map<std::string, SocketValue>      state;          // per-instance runtime memory
    bool                                              bypass = false;
    std::string                                       comments;
};

// ----------------------------------------------------------------------------
// Registry — free functions on a global map (Meyers singleton via function-local
// static, C++11+ thread-safe init). Registration is expected once at startup.
// Pointers returned by find_node_type are stable as long as no further
// register_node_type calls happen. Documented invariant; no enforcement yet.
// ----------------------------------------------------------------------------

inline std::unordered_map<std::string, NodeType>& registry_() {
    static std::unordered_map<std::string, NodeType> r;
    return r;
}

inline void register_node_type(const NodeType& type) {
    registry_()[type.name] = type;
}

inline const NodeType* find_node_type(std::string_view name) {
    auto& r = registry_();
    auto it = r.find(std::string{name});
    return it != r.end() ? &it->second : nullptr;
}

inline const std::unordered_map<std::string, NodeType>& all_node_types() {
    return registry_();
}

// Build a Node instance from a Type. Allocates sockets from the schema,
// copies default_value into each socket's current, populates state from
// state_schema defaults. Position defaults to (0,0); caller can move it
// after construction.
inline Node make_node(const NodeType& type, NodeId id, std::string instance_name) {
    Node n;
    n.id   = id;
    n.type = &type;
    n.name = std::move(instance_name);

    n.inputs.reserve(type.inputs.size());
    for (const auto& spec : type.inputs) {
        Socket s;
        s.name          = spec.name;
        s.type          = spec.type;
        s.direction     = SocketDirection::Input;
        s.default_value = spec.default_value;
        s.current       = spec.default_value;  // current = default until a connection overrides
        s.range         = spec.range;
        n.inputs.push_back(std::move(s));
    }

    n.outputs.reserve(type.outputs.size());
    for (const auto& spec : type.outputs) {
        Socket s;
        s.name          = spec.name;
        s.type          = spec.type;
        s.direction     = SocketDirection::Output;
        s.default_value = spec.default_value;
        s.current       = spec.default_value;  // will be overwritten by evaluate() each tick
        s.range         = spec.range;
        n.outputs.push_back(std::move(s));
    }

    for (const auto& key_spec : type.state_schema) {
        n.state[key_spec.name] = key_spec.default_value;
    }

    return n;
}

}  // namespace substrate
