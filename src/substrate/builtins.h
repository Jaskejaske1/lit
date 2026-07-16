#pragma once

#include <cmath>

#include "graph.h"

// lit — first built-in substrate node types
//
// These are real substrate primitives that live in the library rather than
// in tests or the dev console. Constants anchor the basic value model, and
// Phase is the first real time-based Generator that exercises dt, per-node
// state, init pass, and stable graph ticking.

namespace substrate {

inline void constant_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = self.state.at("value");
}

inline void register_constant_node_type() {
    NodeType t;
    t.name         = "Constant";
    t.display_name = "Constant";
    t.category     = "Generator";
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.state_schema.push_back(StateKeySpec{
        "value", ValueType::Scalar, SocketValue{Scalar{0.5f}}
    });
    t.evaluate = &constant_evaluate;
    register_node_type(t);
}

inline void constant_vec3_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = self.state.at("value");
}

inline void register_constant_vec3_node_type() {
    NodeType t;
    t.name         = "ConstantVec3";
    t.display_name = "Constant Vec3";
    t.category     = "Generator";
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Vec3, SocketValue{Vec3{0.0f, 0.0f, 0.0f}}, std::nullopt
    });
    t.state_schema.push_back(StateKeySpec{
        "value", ValueType::Vec3, SocketValue{Vec3{1.0f, 0.5f, 0.25f}}
    });
    t.evaluate = &constant_vec3_evaluate;
    register_node_type(t);
}

inline void phase_evaluate(Node& self, float dt, float, bool init_pass) {
    Scalar phase = std::get<Scalar>(self.state.at("phase"));
    const Scalar period = std::get<Scalar>(self.inputs[0].current);

    if (!init_pass && period > 0.0f) {
        phase = std::fmod(phase + (dt / period), 1.0f);
        if (phase < 0.0f) {
            phase += 1.0f;
        }
        self.state["phase"] = SocketValue{phase};
    }

    self.outputs[0].current = SocketValue{phase};
}

inline void register_phase_node_type() {
    NodeType t;
    t.name         = "Phase";
    t.display_name = "Phase";
    t.category     = "Generator";
    t.inputs.push_back(SocketSpec{
        "Period", ValueType::Scalar, SocketValue{Scalar{2.0f}}, std::pair{0.01f, 120.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Phase", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.state_schema.push_back(StateKeySpec{
        "phase", ValueType::Scalar, SocketValue{Scalar{0.0f}}
    });
    t.evaluate = &phase_evaluate;
    register_node_type(t);
}

inline void register_builtin_node_types() {
    register_constant_node_type();
    register_constant_vec3_node_type();
    register_phase_node_type();
}

}  // namespace substrate
