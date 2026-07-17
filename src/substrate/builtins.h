#pragma once

#include <algorithm>
#include <cmath>

#include "graph.h"

// lit — first built-in substrate node types
//
// These are real substrate primitives that live in the library rather than
// in tests or the dev console. Constants anchor the basic value model, and
// Phase is the first real time-based Generator that exercises dt, per-node
// state, init pass, and stable graph ticking.

namespace substrate {

inline thread_local Vec3 builtin_probe_position{0.0f, 0.0f, 0.0f};

inline void set_builtin_probe_position(Vec3 position) {
    builtin_probe_position = position;
}

inline Vec3 current_builtin_probe_position() {
    return builtin_probe_position;
}

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

inline void bpm_tap_evaluate(Node& self, float, float, bool) {
    Scalar bpm = std::get<Scalar>(self.state.at("bpm"));
    bpm = std::clamp(bpm, 20.0f, 300.0f);
    self.state["bpm"] = SocketValue{bpm};
    self.outputs[0].current = SocketValue{bpm};
    self.outputs[1].current = SocketValue{60.0f / bpm};
}

inline void add_evaluate(Node& self, float, float, bool) {
    const Scalar a = std::get<Scalar>(self.inputs[0].current);
    const Scalar b = std::get<Scalar>(self.inputs[1].current);
    self.outputs[0].current = SocketValue{a + b};
}

inline void multiply_evaluate(Node& self, float, float, bool) {
    const Scalar a = std::get<Scalar>(self.inputs[0].current);
    const Scalar b = std::get<Scalar>(self.inputs[1].current);
    self.outputs[0].current = SocketValue{a * b};
}

inline void sine_evaluate(Node& self, float, float, bool) {
    constexpr Scalar tau = 6.28318530717958647692f;
    const Scalar input = std::get<Scalar>(self.inputs[0].current);
    self.outputs[0].current = SocketValue{0.5f + 0.5f * std::sin(tau * input)};
}

inline void ramp_evaluate(Node& self, float, float, bool) {
    Scalar input = std::get<Scalar>(self.inputs[0].current);
    input = std::fmod(input, 1.0f);
    if (input < 0.0f) {
        input += 1.0f;
    }
    self.outputs[0].current = SocketValue{input};
}

inline void probe_x_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = SocketValue{builtin_probe_position[0]};
}

inline void probe_y_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = SocketValue{builtin_probe_position[1]};
}

inline void probe_z_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = SocketValue{builtin_probe_position[2]};
}

inline void project2d_evaluate(Node& self, float, float, bool) {
    const Scalar x = std::get<Scalar>(self.inputs[0].current);
    const Scalar y = std::get<Scalar>(self.inputs[1].current);
    const Scalar axis_x = std::get<Scalar>(self.inputs[2].current);
    const Scalar axis_y = std::get<Scalar>(self.inputs[3].current);
    const Scalar offset = std::get<Scalar>(self.inputs[4].current);

    const Scalar axis_length = std::sqrt((axis_x * axis_x) + (axis_y * axis_y));
    if (axis_length <= 0.0001f) {
        self.outputs[0].current = SocketValue{offset};
        return;
    }

    const Scalar normalized_axis_x = axis_x / axis_length;
    const Scalar normalized_axis_y = axis_y / axis_length;
    self.outputs[0].current = SocketValue{
        (x * normalized_axis_x) + (y * normalized_axis_y) + offset
    };
}

inline void project3d_evaluate(Node& self, float, float, bool) {
    const Scalar x = std::get<Scalar>(self.inputs[0].current);
    const Scalar y = std::get<Scalar>(self.inputs[1].current);
    const Scalar z = std::get<Scalar>(self.inputs[2].current);
    const Scalar axis_x = std::get<Scalar>(self.inputs[3].current);
    const Scalar axis_y = std::get<Scalar>(self.inputs[4].current);
    const Scalar axis_z = std::get<Scalar>(self.inputs[5].current);
    const Scalar offset = std::get<Scalar>(self.inputs[6].current);

    const Scalar axis_length = std::sqrt((axis_x * axis_x) + (axis_y * axis_y) + (axis_z * axis_z));
    if (axis_length <= 0.0001f) {
        self.outputs[0].current = SocketValue{offset};
        return;
    }

    const Scalar normalized_axis_x = axis_x / axis_length;
    const Scalar normalized_axis_y = axis_y / axis_length;
    const Scalar normalized_axis_z = axis_z / axis_length;
    self.outputs[0].current = SocketValue{
        (x * normalized_axis_x) + (y * normalized_axis_y) + (z * normalized_axis_z) + offset
    };
}

inline void mix_evaluate(Node& self, float, float, bool) {
    const Scalar a = std::get<Scalar>(self.inputs[0].current);
    const Scalar b = std::get<Scalar>(self.inputs[1].current);
    const Scalar t = std::clamp(std::get<Scalar>(self.inputs[2].current), 0.0f, 1.0f);
    self.outputs[0].current = SocketValue{a + (b - a) * t};
}

inline void mix_vec3_evaluate(Node& self, float, float, bool) {
    const Vec3 a = std::get<Vec3>(self.inputs[0].current);
    const Vec3 b = std::get<Vec3>(self.inputs[1].current);
    const Scalar t = std::clamp(std::get<Scalar>(self.inputs[2].current), 0.0f, 1.0f);
    self.outputs[0].current = SocketValue{Vec3{
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
    }};
}

inline void clamp_evaluate(Node& self, float, float, bool) {
    const Scalar value = std::get<Scalar>(self.inputs[0].current);
    Scalar min_value = std::get<Scalar>(self.inputs[1].current);
    Scalar max_value = std::get<Scalar>(self.inputs[2].current);
    if (min_value > max_value) {
        std::swap(min_value, max_value);
    }
    self.outputs[0].current = SocketValue{std::clamp(value, min_value, max_value)};
}

inline void time_offset_evaluate(Node& self, float, float, bool) {
    Scalar value = std::get<Scalar>(self.inputs[0].current);
    value += std::get<Scalar>(self.inputs[1].current);
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    self.outputs[0].current = SocketValue{value};
}

inline void spatial_mirror_evaluate(Node& self, float, float, bool) {
    const Scalar position = std::get<Scalar>(self.inputs[0].current);
    const Scalar center = std::get<Scalar>(self.inputs[1].current);
    const Scalar half_width = std::max(std::get<Scalar>(self.inputs[2].current), 0.0001f);
    const Scalar mirrored = std::clamp(std::abs(position - center) / half_width, 0.0f, 1.0f);
    self.outputs[0].current = SocketValue{mirrored};
}

inline void decay_evaluate(Node& self, float dt, float, bool init_pass) {
    const Scalar input = std::clamp(std::get<Scalar>(self.inputs[0].current), 0.0f, 1.0f);
    const Scalar tau = std::max(std::get<Scalar>(self.inputs[1].current), 0.0f);
    Scalar previous_value = std::clamp(std::get<Scalar>(self.state.at("prev_value")), 0.0f, 1.0f);

    if (!init_pass && tau > 0.0f) {
        previous_value *= std::exp(-dt / tau);
    } else if (!init_pass) {
        previous_value = 0.0f;
    }

    const Scalar output = std::max(input, previous_value);
    self.state["prev_value"] = SocketValue{output};
    self.outputs[0].current = SocketValue{output};
}

inline void output_dimmer_evaluate(Node& self, float, float, bool) {
    const Scalar level = std::clamp(std::get<Scalar>(self.inputs[0].current), 0.0f, 1.0f);
    self.outputs[0].current = SocketValue{level};
}

inline void output_tilt_evaluate(Node& self, float, float, bool) {
    const Scalar tilt = std::clamp(std::get<Scalar>(self.inputs[0].current), 0.0f, 1.0f);
    self.outputs[0].current = SocketValue{tilt};
}

inline void spatial_fixture_driver_evaluate(Node& self, float, float, bool) {
    const Scalar dimmer = std::clamp(std::get<Scalar>(self.inputs[0].current), 0.0f, 1.0f);
    const Scalar tilt = std::clamp(std::get<Scalar>(self.inputs[1].current), 0.0f, 1.0f);
    const Vec3 raw_color = std::get<Vec3>(self.inputs[2].current);
    const Vec3 color{
        std::clamp(raw_color[0], 0.0f, 1.0f),
        std::clamp(raw_color[1], 0.0f, 1.0f),
        std::clamp(raw_color[2], 0.0f, 1.0f),
    };
    self.outputs[0].current = SocketValue{dimmer};
    self.outputs[1].current = SocketValue{tilt};
    self.outputs[2].current = SocketValue{color};
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

inline void register_bpm_tap_node_type() {
    NodeType t;
    t.name         = "BPMTap";
    t.display_name = "BPM Tap";
    t.category     = "Generator";
    t.outputs.push_back(SocketSpec{
        "BPM", ValueType::Scalar, SocketValue{Scalar{120.0f}}, std::pair{20.0f, 300.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Period", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::pair{0.2f, 3.0f}
    });
    t.state_schema.push_back(StateKeySpec{
        "bpm", ValueType::Scalar, SocketValue{Scalar{120.0f}}
    });
    t.evaluate = &bpm_tap_evaluate;
    register_node_type(t);
}

inline void register_add_node_type() {
    NodeType t;
    t.name         = "Add";
    t.display_name = "Add";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "A", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "B", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Sum", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &add_evaluate;
    register_node_type(t);
}

inline void register_multiply_node_type() {
    NodeType t;
    t.name         = "Multiply";
    t.display_name = "Multiply";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "A", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "B", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Product", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &multiply_evaluate;
    register_node_type(t);
}

inline void register_sine_node_type() {
    NodeType t;
    t.name         = "Sine";
    t.display_name = "Sine";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "Phase", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Wave", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::nullopt
    });
    t.evaluate = &sine_evaluate;
    register_node_type(t);
}

inline void register_ramp_node_type() {
    NodeType t;
    t.name         = "Ramp";
    t.display_name = "Ramp";
    t.category     = "Generator";
    t.inputs.push_back(SocketSpec{
        "Phase", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Wave", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &ramp_evaluate;
    register_node_type(t);
}

inline void register_probe_x_node_type() {
    NodeType t;
    t.name         = "ProbeX";
    t.display_name = "Probe X";
    t.category     = "Spatial";
    t.outputs.push_back(SocketSpec{
        "X", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &probe_x_evaluate;
    register_node_type(t);
}

inline void register_probe_y_node_type() {
    NodeType t;
    t.name         = "ProbeY";
    t.display_name = "Probe Y";
    t.category     = "Spatial";
    t.outputs.push_back(SocketSpec{
        "Y", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &probe_y_evaluate;
    register_node_type(t);
}

inline void register_probe_z_node_type() {
    NodeType t;
    t.name         = "ProbeZ";
    t.display_name = "Probe Z";
    t.category     = "Spatial";
    t.outputs.push_back(SocketSpec{
        "Z", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &probe_z_evaluate;
    register_node_type(t);
}

inline void register_project2d_node_type() {
    NodeType t;
    t.name         = "Project2D";
    t.display_name = "Project 2D";
    t.category     = "Spatial";
    t.inputs.push_back(SocketSpec{
        "X", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Y", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "AxisX", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "AxisY", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Offset", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Coordinate", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &project2d_evaluate;
    register_node_type(t);
}

inline void register_project3d_node_type() {
    NodeType t;
    t.name         = "Project3D";
    t.display_name = "Project 3D";
    t.category     = "Spatial";
    t.inputs.push_back(SocketSpec{
        "X", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Y", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Z", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "AxisX", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "AxisY", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "AxisZ", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Offset", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Coordinate", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &project3d_evaluate;
    register_node_type(t);
}

inline void register_mix_node_type() {
    NodeType t;
    t.name         = "Mix";
    t.display_name = "Mix";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "A", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "B", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "T", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::pair{0.0f, 1.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &mix_evaluate;
    register_node_type(t);
}

inline void register_mix_vec3_node_type() {
    NodeType t;
    t.name         = "MixVec3";
    t.display_name = "Mix Vec3";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "A", ValueType::Vec3, SocketValue{Vec3{0.0f, 0.0f, 0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "B", ValueType::Vec3, SocketValue{Vec3{1.0f, 1.0f, 1.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "T", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::pair{0.0f, 1.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Vec3, SocketValue{Vec3{0.0f, 0.0f, 0.0f}}, std::nullopt
    });
    t.evaluate = &mix_vec3_evaluate;
    register_node_type(t);
}

inline void register_clamp_node_type() {
    NodeType t;
    t.name         = "Clamp";
    t.display_name = "Clamp";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Min", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Max", ValueType::Scalar, SocketValue{Scalar{1.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &clamp_evaluate;
    register_node_type(t);
}

inline void register_time_offset_node_type() {
    NodeType t;
    t.name         = "TimeOffset";
    t.display_name = "Time Offset";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "Signal", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Offset", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{-1.0f, 1.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Shifted", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &time_offset_evaluate;
    register_node_type(t);
}

inline void register_spatial_mirror_node_type() {
    NodeType t;
    t.name         = "SpatialMirror";
    t.display_name = "Spatial Mirror";
    t.category     = "Spatial";
    t.inputs.push_back(SocketSpec{
        "Position", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "Center", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::nullopt
    });
    t.inputs.push_back(SocketSpec{
        "HalfWidth", ValueType::Scalar, SocketValue{Scalar{0.5f}}, std::pair{0.0001f, 1000.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Mirrored", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &spatial_mirror_evaluate;
    register_node_type(t);
}

inline void register_decay_node_type() {
    NodeType t;
    t.name         = "Decay";
    t.display_name = "Decay";
    t.category     = "Modifier";
    t.inputs.push_back(SocketSpec{
        "Input", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{0.0f, 1.0f}
    });
    t.inputs.push_back(SocketSpec{
        "Tau", ValueType::Scalar, SocketValue{Scalar{0.35f}}, std::pair{0.0f, 10.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.state_schema.push_back(StateKeySpec{
        "prev_value", ValueType::Scalar, SocketValue{Scalar{0.0f}}
    });
    t.evaluate = &decay_evaluate;
    register_node_type(t);
}

inline void register_output_dimmer_node_type() {
    NodeType t;
    t.name         = "OutputDimmer";
    t.display_name = "Output Dimmer";
    t.category     = "Output";
    t.inputs.push_back(SocketSpec{
        "Level", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{0.0f, 1.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Dimmer", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &output_dimmer_evaluate;
    register_node_type(t);
}

inline void register_output_tilt_node_type() {
    NodeType t;
    t.name         = "OutputTilt";
    t.display_name = "Output Tilt";
    t.category     = "Output";
    t.inputs.push_back(SocketSpec{
        "Tilt", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{0.0f, 1.0f}
    });
    t.outputs.push_back(SocketSpec{
        "Tilt", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.evaluate = &output_tilt_evaluate;
    register_node_type(t);
}

inline void register_spatial_fixture_driver_node_type() {
    NodeType t;
    t.name         = "SpatialFixtureDriver";
    t.display_name = "Spatial Fixture Driver";
    t.category     = "Output";
    t.inputs.push_back(SocketSpec{
        "Dimmer", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{0.0f, 1.0f}
    });
    t.inputs.push_back(SocketSpec{
        "Tilt", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::pair{0.0f, 1.0f}
    });
    t.inputs.push_back(SocketSpec{
        "ColorRGB", ValueType::Vec3, SocketValue{Vec3{1.0f, 1.0f, 1.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Dimmer", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "Tilt", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.outputs.push_back(SocketSpec{
        "ColorRGB", ValueType::Vec3, SocketValue{Vec3{1.0f, 1.0f, 1.0f}}, std::nullopt
    });
    t.evaluate = &spatial_fixture_driver_evaluate;
    register_node_type(t);
}

inline void register_builtin_node_types() {
    register_constant_node_type();
    register_constant_vec3_node_type();
    register_bpm_tap_node_type();
    register_phase_node_type();
    register_add_node_type();
    register_multiply_node_type();
    register_sine_node_type();
    register_ramp_node_type();
    register_probe_x_node_type();
    register_probe_y_node_type();
    register_probe_z_node_type();
    register_project2d_node_type();
    register_project3d_node_type();
    register_mix_node_type();
    register_mix_vec3_node_type();
    register_clamp_node_type();
    register_time_offset_node_type();
    register_spatial_mirror_node_type();
    register_decay_node_type();
    register_output_dimmer_node_type();
    register_output_tilt_node_type();
    register_spatial_fixture_driver_node_type();
}

}  // namespace substrate
