#pragma once

#include <vector>
#include <substrate/substrate.h>

struct ProbeRenderData {
    substrate::Vec3 position;
    substrate::Vec3 color;   // RGB
};

class ProbeManager {
public:
    ProbeManager() = default;

    // Initialize with a reference graph and the driver node ID.
    void initialize(const substrate::Graph& source_graph, substrate::NodeId driver_node_id);

    // Update all probe copies (tick and extract colors).
    void update(float dt);

    const std::vector<ProbeRenderData>& get_render_data() const { return render_data_; }

private:
    std::vector<substrate::FixtureProbe> probes_;
    std::vector<substrate::Graph>        per_probe_graphs_;
    substrate::NodeId                    driver_node_id_ = 0;
    std::vector<ProbeRenderData>         render_data_;
};
