#pragma once

#include <cmath>

#include "graph.h"

// lit — first built-in substrate node types
//
// These are real substrate primitives that live in the library rather than
// in tests or the dev console. The first one is Phase because it is the
// smallest time-based Generator in the docs and it exercises exactly what the
// graph core was added for: dt, elapsed time, per-node state, init pass, and
// stable ticking.

namespace substrate {

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

}  // namespace substrate
