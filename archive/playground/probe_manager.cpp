#include "probe_manager.h"
#include <cassert>

using namespace substrate;

void ProbeManager::initialize(const Graph& source_graph, NodeId driver_node_id) {
    driver_node_id_ = driver_node_id;
    probes_.clear();
    per_probe_graphs_.clear();
    render_data_.clear();

    // Create the default bar probes (same as in lit_view).
    FixtureId next_fixture_id = 1;
    probes_ = make_default_preview_probes(next_fixture_id);

    // Deep‑copy the graph for each probe and bake it.
    per_probe_graphs_.reserve(probes_.size());
    for (const auto& probe : probes_) {
        Graph copy = source_graph;
        // We must bake the copy manually; the source is already baked.
        GraphBuildError err = copy.bake();
        assert(err.code == GraphBuildErrorCode::None);
        (void)err;

        // Set the probe position and run init pass once.
        set_builtin_probe_position(probe.position);
        bool ok = copy.init_pass();
        assert(ok);
        (void)ok;
        per_probe_graphs_.push_back(std::move(copy));
    }

    // restore default probe position (0,0,0)
    set_builtin_probe_position(Vec3{0.0f, 0.0f, 0.0f});

    // Pre‑allocate render data.
    render_data_.resize(probes_.size());
}

void ProbeManager::update(float dt) {
    for (size_t i = 0; i < probes_.size(); ++i) {
        Graph& graph = per_probe_graphs_[i];
        const Vec3 pos = probes_[i].position;

        set_builtin_probe_position(pos);
        GraphBuildError err;
        if (!graph.tick(dt, &err)) {
            // This should never happen in normal operation.
            fprintf(stderr, "Probe %zu tick error: %s\n", i, graph_build_error_name(err.code).data());
            continue;
        }

        // Extract color from the SpatialFixtureDriver node.
        const Node* driver = graph.find_node(driver_node_id_);
        if (driver && driver->outputs.size() >= 3) {
            const Vec3 raw = std::get<Vec3>(driver->outputs[2].current);
            render_data_[i].position = pos;
            // Clamp to valid range for display.
            render_data_[i].color = Vec3{
                std::clamp(raw[0], 0.0f, 1.0f),
                std::clamp(raw[1], 0.0f, 1.0f),
                std::clamp(raw[2], 0.0f, 1.0f)
            };
        }
    }
    set_builtin_probe_position(Vec3{0.0f, 0.0f, 0.0f});
}
