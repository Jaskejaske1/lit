#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "builtins.h"
#include "fixture.h"
#include "graph.h"

namespace substrate {

struct DefaultSpatialPatchIds {
    NodeId spatial_fixture_driver_node_id = 0;
    NodeId preview_node_id = 0;
    int preview_output_socket_index = 0;
};

inline NodeId spawn_named_node(Graph& graph,
                               NodeId& next_id,
                               const char* type_name,
                               std::string instance_name) {
    const NodeType* type = find_node_type(type_name);
    if (!type) {
        return 0;
    }

    const NodeId id = next_id++;
    graph.nodes.push_back(make_node(*type, id, std::move(instance_name)));
    graph.mark_dirty();
    return id;
}

inline bool add_connection(Graph& graph,
                           ConnectionId& next_connection_id,
                           NodeId source_node_id,
                           std::size_t source_socket_index,
                           NodeId destination_node_id,
                           std::size_t destination_socket_index) {
    graph.connections.push_back(Connection{
        next_connection_id++,
        SocketEndpoint{source_node_id, source_socket_index},
        SocketEndpoint{destination_node_id, destination_socket_index},
    });

    GraphBuildError error = graph.bake();
    if (error.code == GraphBuildErrorCode::None) {
        return true;
    }

    graph.connections.pop_back();
    graph.mark_dirty();
    return false;
}

inline bool seed_default_spatial_patch(Graph& graph,
                                       NodeId& next_id,
                                       ConnectionId& next_connection_id,
                                       DefaultSpatialPatchIds* ids = nullptr) {
    const NodeId probe_x_id = spawn_named_node(graph, next_id, "ProbeX", "Probe X");
    const NodeId probe_y_id = spawn_named_node(graph, next_id, "ProbeY", "Probe Y");
    const NodeId probe_z_id = spawn_named_node(graph, next_id, "ProbeZ", "Probe Z");
    const NodeId mirror_x_id = spawn_named_node(graph, next_id, "SpatialMirror", "Mirror Symmetry");
    const NodeId project3d_id = spawn_named_node(graph, next_id, "Project3D", "Bar Coordinate");
    const NodeId bpm_tap_id = spawn_named_node(graph, next_id, "BPMTap", "Sweep Tempo");
    const NodeId beats_per_sweep_id = spawn_named_node(graph, next_id, "Constant", "Beats Per Sweep");
    const NodeId sweep_period_id = spawn_named_node(graph, next_id, "Multiply", "Sweep Period");
    const NodeId phase_id = spawn_named_node(graph, next_id, "Phase", "Sweep Phase");
    const NodeId time_offset_id = spawn_named_node(graph, next_id, "TimeOffset", "Phase Offset");
    const NodeId band_id = spawn_named_node(graph, next_id, "Band", "Sweep Band");
    const NodeId decay_id = spawn_named_node(graph, next_id, "Decay", "Trail Decay");
    const NodeId base_tilt_id = spawn_named_node(graph, next_id, "Constant", "Base Tilt");
    const NodeId peak_tilt_id = spawn_named_node(graph, next_id, "Constant", "Peak Tilt");
    const NodeId tilt_mix_id = spawn_named_node(graph, next_id, "Mix", "Tilt Mix");
    const NodeId full_intensity_id = spawn_named_node(graph, next_id, "Constant", "Full Intensity");
    const NodeId white_id = spawn_named_node(graph, next_id, "ConstantVec3", "Base White");
    const NodeId red_id = spawn_named_node(graph, next_id, "ConstantVec3", "Sweep Red");
    const NodeId color_mix_id = spawn_named_node(graph, next_id, "MixVec3", "Color Mix");
    const NodeId fixture_driver_id = spawn_named_node(graph, next_id, "SpatialFixtureDriver", "Fixture Driver");

    if (!probe_x_id || !probe_y_id || !probe_z_id || !mirror_x_id || !project3d_id ||
        !bpm_tap_id || !beats_per_sweep_id || !sweep_period_id || !phase_id ||
        !time_offset_id || !band_id || !decay_id || !base_tilt_id || !peak_tilt_id ||
        !tilt_mix_id || !full_intensity_id || !white_id || !red_id ||
        !color_mix_id || !fixture_driver_id) {
        return false;
    }

    if (Node* project3d = graph.find_node(project3d_id)) {
        project3d->inputs[3].default_value = SocketValue{Scalar{0.47f}};
        project3d->inputs[3].current = SocketValue{Scalar{0.47f}};
        project3d->inputs[4].default_value = SocketValue{Scalar{0.88f}};
        project3d->inputs[4].current = SocketValue{Scalar{0.88f}};
        project3d->inputs[5].default_value = SocketValue{Scalar{0.18f}};
        project3d->inputs[5].current = SocketValue{Scalar{0.18f}};
    }
    if (Node* bpm_tap = graph.find_node(bpm_tap_id)) {
        bpm_tap->state["bpm"] = SocketValue{Scalar{100.0f}};
    }
    if (Node* beats_per_sweep = graph.find_node(beats_per_sweep_id)) {
        beats_per_sweep->state["value"] = SocketValue{Scalar{3.0f}};
    }
    if (Node* decay = graph.find_node(decay_id)) {
        decay->inputs[1].default_value = SocketValue{Scalar{0.28f}};
        decay->inputs[1].current = SocketValue{Scalar{0.28f}};
    }
    if (Node* band = graph.find_node(band_id)) {
        band->inputs[1].default_value = SocketValue{Scalar{0.0f}};
        band->inputs[1].current = SocketValue{Scalar{0.0f}};
        band->inputs[2].default_value = SocketValue{Scalar{0.10f}};
        band->inputs[2].current = SocketValue{Scalar{0.10f}};
        band->inputs[3].default_value = SocketValue{Scalar{0.10f}};
        band->inputs[3].current = SocketValue{Scalar{0.10f}};
    }
    if (Node* base_tilt = graph.find_node(base_tilt_id)) {
        base_tilt->state["value"] = SocketValue{Scalar{0.30f}};
    }
    if (Node* peak_tilt = graph.find_node(peak_tilt_id)) {
        peak_tilt->state["value"] = SocketValue{Scalar{0.82f}};
    }
    if (Node* full_intensity = graph.find_node(full_intensity_id)) {
        full_intensity->state["value"] = SocketValue{Scalar{1.0f}};
    }
    if (Node* white = graph.find_node(white_id)) {
        white->state["value"] = SocketValue{Vec3{1.0f, 1.0f, 1.0f}};
    }
    if (Node* red = graph.find_node(red_id)) {
        red->state["value"] = SocketValue{Vec3{1.0f, 0.0f, 0.0f}};
    }

    if (!add_connection(graph, next_connection_id, probe_x_id, 0, mirror_x_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, mirror_x_id, 0, project3d_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, probe_y_id, 0, project3d_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, probe_z_id, 0, project3d_id, 2)) return false;
    if (!add_connection(graph, next_connection_id, bpm_tap_id, 1, sweep_period_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, beats_per_sweep_id, 0, sweep_period_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, sweep_period_id, 0, phase_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, phase_id, 0, time_offset_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, project3d_id, 0, time_offset_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, time_offset_id, 0, band_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, band_id, 0, decay_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, base_tilt_id, 0, tilt_mix_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, peak_tilt_id, 0, tilt_mix_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, decay_id, 0, tilt_mix_id, 2)) return false;
    if (!add_connection(graph, next_connection_id, white_id, 0, color_mix_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, red_id, 0, color_mix_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, decay_id, 0, color_mix_id, 2)) return false;
    if (!add_connection(graph, next_connection_id, full_intensity_id, 0, fixture_driver_id, 0)) return false;
    if (!add_connection(graph, next_connection_id, tilt_mix_id, 0, fixture_driver_id, 1)) return false;
    if (!add_connection(graph, next_connection_id, color_mix_id, 0, fixture_driver_id, 2)) return false;

    if (ids) {
        ids->spatial_fixture_driver_node_id = fixture_driver_id;
        ids->preview_node_id = fixture_driver_id;
        ids->preview_output_socket_index = 2;
    }

    return true;
}

inline std::vector<FixtureProbe> make_default_preview_probes(FixtureId& next_fixture_id) {
    const std::vector<Vec3> positions = {
        Vec3{0.32f, 0.15f, -0.05f},
        Vec3{0.28f, 0.30f, -0.025f},
        Vec3{0.24f, 0.45f, 0.0f},
        Vec3{0.20f, 0.60f, 0.025f},
        Vec3{0.16f, 0.75f, 0.05f},
        Vec3{0.68f, 0.15f, -0.05f},
        Vec3{0.72f, 0.30f, -0.025f},
        Vec3{0.76f, 0.45f, 0.0f},
        Vec3{0.80f, 0.60f, 0.025f},
        Vec3{0.84f, 0.75f, 0.05f},
    };
    const std::vector<std::string> names = {
        "Bar L1", "Bar L2", "Bar L3", "Bar L4", "Bar L5",
        "Bar R1", "Bar R2", "Bar R3", "Bar R4", "Bar R5",
    };

    std::vector<FixtureProbe> probes;
    probes.reserve(names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        probes.push_back(FixtureProbe{
            next_fixture_id++,
            names[i],
            positions[i],
            { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB },
        });
    }
    return probes;
}

}  // namespace substrate
