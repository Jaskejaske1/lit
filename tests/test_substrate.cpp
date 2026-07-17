// test_substrate — small concrete test for the substrate skeleton.
//
// Exercises the design end-to-end:
//   1. Register two NodeTypes (Constant = Scalar, ConstantVec3 = Vec3)
//   2. Look them up via the registry
//   3. Build Node instances via make_node()
//   4. Verify sockets are populated from the spec
//   5. Verify state is populated from state_schema defaults
//   6. Run evaluate() and verify output.current reflects state
//   7. Verify find_node_type returns nullptr for unknown names
//
// If everything passes, the design works. If something fails, an assert fires
// (Debug build) or std::get throws (Release build) — either way, non-zero exit.

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <substrate/substrate.h>

using namespace substrate;

// ----------------------------------------------------------------------------
// Test types
// ----------------------------------------------------------------------------

void tick_counter_evaluate(Node& self, float, float, bool init_pass) {
    Scalar ticks = std::get<Scalar>(self.state.at("ticks"));
    if (!init_pass) {
        ticks += 1.0f;
        self.state["ticks"] = SocketValue{ticks};
    }
    self.outputs[0].current = SocketValue{ticks};
}

void register_tick_counter() {
    NodeType t;
    t.name         = "TickCounter";
    t.display_name = "Tick Counter";
    t.category     = "Generator";
    t.outputs.push_back(SocketSpec{
        "Ticks", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt
    });
    t.state_schema.push_back(StateKeySpec{
        "ticks", ValueType::Scalar, SocketValue{Scalar{0.0f}}
    });
    t.evaluate = &tick_counter_evaluate;
    register_node_type(t);
}

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL [" << __LINE__ << "]: " #cond "\n"; \
        return 1; \
    } \
} while (0)

#define PASS(msg) std::cout << "[PASS] " msg "\n"

struct DriverOutputs {
    float dimmer = 0.0f;
    float tilt = 0.0f;
    Vec3 color{0.0f, 0.0f, 0.0f};
};

const FixtureProbe* find_fixture_probe(const std::vector<FixtureProbe>& probes, std::string_view name) {
    for (const auto& probe : probes) {
        if (probe.name == name) {
            return &probe;
        }
    }
    return nullptr;
}

DriverOutputs evaluate_driver_outputs_at_probe(const Graph& source_graph,
                                               NodeId driver_node_id,
                                               Vec3 position,
                                               int steps,
                                               float dt) {
    Graph graph = source_graph;
    GraphBuildError error;
    const Vec3 previous_position = current_builtin_probe_position();
    set_builtin_probe_position(position);
    const bool init_ok = graph.init_pass(&error);
    assert(init_ok);
    (void)init_ok;
    for (int i = 0; i < steps; ++i) {
        set_builtin_probe_position(position);
        const bool tick_ok = graph.tick(dt, &error);
        assert(tick_ok);
        (void)tick_ok;
    }

    const Node* driver = graph.find_node(driver_node_id);
    assert(driver != nullptr);
    DriverOutputs outputs;
    outputs.dimmer = std::get<Scalar>(driver->outputs[0].current);
    outputs.tilt = std::get<Scalar>(driver->outputs[1].current);
    outputs.color = std::get<Vec3>(driver->outputs[2].current);
    set_builtin_probe_position(previous_position);
    return outputs;
}

float color_distance(Vec3 a, Vec3 b) {
    return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
}

int test_register_and_find() {
    register_builtin_node_types();

    const NodeType* c = find_node_type("Constant");
    CHECK(c != nullptr);
    CHECK(c->name == "Constant");
    CHECK(c->display_name == "Constant");
    CHECK(c->category == "Generator");
    CHECK(c->inputs.size() == 0);
    CHECK(c->outputs.size() == 1);
    CHECK(c->outputs[0].name == "Value");
    CHECK(c->outputs[0].type == ValueType::Scalar);
    CHECK(c->state_schema.size() == 1);
    CHECK(c->state_schema[0].name == "value");
    CHECK(std::get<Scalar>(c->state_schema[0].default_value) == 0.5f);

    PASS("register_node_type + find_node_type (Scalar)");
    return 0;
}

int test_find_unknown_returns_null() {
    CHECK(find_node_type("DoesNotExist") == nullptr);
    PASS("find_node_type returns nullptr for unknown name");
    return 0;
}

int test_make_node_populates_outputs() {
    const NodeType* c = find_node_type("Constant");
    Node n = make_node(*c, 42, "My Constant");
    CHECK(n.id == 42);
    CHECK(n.name == "My Constant");
    CHECK(n.type == c);
    CHECK(n.inputs.size() == 0);
    CHECK(n.outputs.size() == 1);
    CHECK(n.outputs[0].name == "Value");
    CHECK(n.outputs[0].type == ValueType::Scalar);
    CHECK(n.outputs[0].direction == SocketDirection::Output);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.0f);

    PASS("make_node populates outputs from SocketSpec");
    return 0;
}

int test_make_node_populates_state() {
    const NodeType* c = find_node_type("Constant");
    Node n = make_node(*c, 1, "x");
    CHECK(n.state.size() == 1);
    CHECK(n.state.count("value") == 1);
    CHECK(std::get<Scalar>(n.state["value"]) == 0.5f);

    PASS("make_node populates state from state_schema");
    return 0;
}

int test_evaluate_writes_output() {
    const NodeType* c = find_node_type("Constant");
    Node n = make_node(*c, 1, "x");

    // Simulate the engine calling evaluate().
    n.type->evaluate(n, 0.016f, 0.0f, false);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.5f);

    // Mutate state, evaluate again — output follows.
    n.state["value"] = SocketValue{Scalar{0.75f}};
    n.type->evaluate(n, 0.016f, 0.0f, false);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.75f);

    // init_pass flag should not affect this type (it doesn't use time).
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.75f);

    PASS("evaluate() writes to output.current, follows state changes");
    return 0;
}

int test_vec3_through_variant() {
    const NodeType* cv3 = find_node_type("ConstantVec3");
    CHECK(cv3 != nullptr);
    CHECK(cv3->outputs[0].type == ValueType::Vec3);

    Node n = make_node(*cv3, 7, "Vec3 Const");
    CHECK(n.outputs[0].type == ValueType::Vec3);
    CHECK(std::get<Vec3>(n.state["value"])[0] == 1.0f);
    CHECK(std::get<Vec3>(n.state["value"])[1] == 0.5f);
    CHECK(std::get<Vec3>(n.state["value"])[2] == 0.25f);

    n.type->evaluate(n, 0, 0, false);
    Vec3 result = std::get<Vec3>(n.outputs[0].current);
    CHECK(result[0] == 1.0f);
    CHECK(result[1] == 0.5f);
    CHECK(result[2] == 0.25f);

    PASS("Vec3 type flows through SocketValue variant");
    return 0;
}

int test_fixture_probe_trait_helpers() {
    FixtureProbe probe{
        501,
        "Probe A",
        Vec3{0.1f, 0.2f, 0.3f},
        {},
    };

    CHECK(!fixture_has_trait(probe, FixtureTrait::Dimmer));
    fixture_set_trait(probe, FixtureTrait::Dimmer, true);
    fixture_set_trait(probe, FixtureTrait::Pan, true);
    CHECK(fixture_has_trait(probe, FixtureTrait::Dimmer));
    CHECK(fixture_has_trait(probe, FixtureTrait::Pan));
    CHECK(probe.traits.size() == 2);
    CHECK(fixture_trait_name(FixtureTrait::Tilt) == "Tilt");

    fixture_set_trait(probe, FixtureTrait::Dimmer, false);
    CHECK(!fixture_has_trait(probe, FixtureTrait::Dimmer));
    CHECK(fixture_has_trait(probe, FixtureTrait::Pan));

    PASS("Fixture probe helpers manage capability traits");
    return 0;
}

int test_all_node_types() {
    const auto& all = all_node_types();
    CHECK(all.size() >= 23);
    CHECK(all.count("BPMTap") == 1);
    CHECK(all.count("Constant") == 1);
    CHECK(all.count("ConstantVec3") == 1);
    CHECK(all.count("Mix") == 1);
    CHECK(all.count("MixVec3") == 1);
    CHECK(all.count("Clamp") == 1);
    CHECK(all.count("TimeOffset") == 1);
    CHECK(all.count("SpatialMirror") == 1);
    CHECK(all.count("Decay") == 1);
    CHECK(all.count("OutputDimmer") == 1);
    CHECK(all.count("OutputTilt") == 1);
    CHECK(all.count("SpatialFixtureDriver") == 1);
    CHECK(all.count("Multiply") == 1);
    CHECK(all.count("Sine") == 1);
    CHECK(all.count("Ramp") == 1);
    CHECK(all.count("ProbeX") == 1);
    CHECK(all.count("ProbeY") == 1);
    CHECK(all.count("ProbeZ") == 1);
    CHECK(all.count("Project2D") == 1);
    CHECK(all.count("Project3D") == 1);
    CHECK(all.count("Band") == 1);

    PASS("all_node_types() enumerates registry");
    return 0;
}

int test_graph_propagates_connections_in_topological_order() {
    const NodeType* c = find_node_type("Constant");
    const NodeType* a = find_node_type("Add");
    CHECK(c != nullptr);
    CHECK(a != nullptr);

    Graph g;

    Node source = make_node(*c, 1, "Const");
    source.state["value"] = SocketValue{Scalar{0.5f}};

    Node mid = make_node(*a, 2, "Add A");
    mid.inputs[1].default_value = SocketValue{Scalar{1.0f}};
    mid.inputs[1].current = SocketValue{Scalar{1.0f}};

    Node sink = make_node(*a, 3, "Add B");
    sink.inputs[1].default_value = SocketValue{Scalar{2.0f}};
    sink.inputs[1].current = SocketValue{Scalar{2.0f}};

    g.nodes.push_back(source);
    g.nodes.push_back(mid);
    g.nodes.push_back(sink);
    g.connections.push_back(Connection{1, {1, 0}, {2, 0}});
    g.connections.push_back(Connection{2, {2, 0}, {3, 0}});

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::None);
    CHECK(g.evaluation_order.size() == 3);
    CHECK(g.init_pass());

    CHECK(std::get<Scalar>(g.find_node(1)->outputs[0].current) == 0.5f);
    CHECK(std::get<Scalar>(g.find_node(2)->inputs[0].current) == 0.5f);
    CHECK(std::get<Scalar>(g.find_node(2)->outputs[0].current) == 1.5f);
    CHECK(std::get<Scalar>(g.find_node(3)->inputs[0].current) == 1.5f);
    CHECK(std::get<Scalar>(g.find_node(3)->outputs[0].current) == 3.5f);

    CHECK(g.tick(0.016f));
    CHECK(std::get<Scalar>(g.find_node(3)->outputs[0].current) == 3.5f);

    PASS("Graph bakes stable topo order and propagates connection values");
    return 0;
}

int test_graph_uses_default_input_when_disconnected() {
    const NodeType* a = find_node_type("Add");
    CHECK(a != nullptr);

    Graph g;
    Node add = make_node(*a, 11, "Add Solo");
    add.inputs[0].default_value = SocketValue{Scalar{1.0f}};
    add.inputs[0].current = SocketValue{Scalar{1.0f}};
    add.inputs[1].default_value = SocketValue{Scalar{0.25f}};
    add.inputs[1].current = SocketValue{Scalar{0.25f}};
    g.nodes.push_back(add);

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::None);
    CHECK(g.init_pass());

    CHECK(std::get<Scalar>(g.find_node(11)->inputs[0].current) == 1.0f);
    CHECK(std::get<Scalar>(g.find_node(11)->outputs[0].current) == 1.25f);

    PASS("Graph leaves disconnected inputs on their default value");
    return 0;
}

int test_graph_rejects_cycles() {
    const NodeType* a = find_node_type("Add");
    CHECK(a != nullptr);

    Graph g;
    g.nodes.push_back(make_node(*a, 21, "Add A"));
    g.nodes.push_back(make_node(*a, 22, "Add B"));
    g.connections.push_back(Connection{1, {21, 0}, {22, 0}});
    g.connections.push_back(Connection{2, {22, 0}, {21, 0}});

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::CycleDetected);

    PASS("Graph rejects cyclic topologies at bake time");
    return 0;
}

int test_graph_rejects_multiple_sources_to_one_input() {
    const NodeType* c = find_node_type("Constant");
    const NodeType* a = find_node_type("Add");
    CHECK(c != nullptr);
    CHECK(a != nullptr);

    Graph g;
    g.nodes.push_back(make_node(*c, 51, "Const A"));
    g.nodes.push_back(make_node(*c, 52, "Const B"));
    g.nodes.push_back(make_node(*a, 53, "Add"));
    g.connections.push_back(Connection{1, {51, 0}, {53, 0}});
    g.connections.push_back(Connection{2, {52, 0}, {53, 0}});

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::DestinationAlreadyConnected);

    PASS("Graph rejects multiple source connections into one input socket");
    return 0;
}

int test_graph_init_pass_does_not_advance_time_sensitive_state() {
    register_tick_counter();

    const NodeType* counter = find_node_type("TickCounter");
    CHECK(counter != nullptr);

    Graph g;
    g.nodes.push_back(make_node(*counter, 31, "Counter"));

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::None);
    CHECK(g.init_pass());

    CHECK(std::get<Scalar>(g.find_node(31)->state["ticks"]) == 0.0f);
    CHECK(std::get<Scalar>(g.find_node(31)->outputs[0].current) == 0.0f);

    CHECK(g.tick(0.016f));
    CHECK(std::get<Scalar>(g.find_node(31)->state["ticks"]) == 1.0f);
    CHECK(std::get<Scalar>(g.find_node(31)->outputs[0].current) == 1.0f);
    CHECK(g.elapsed_seconds > 0.0f);

    PASS("Graph init_pass seeds outputs without advancing time-sensitive state");
    return 0;
}

int test_phase_node_advances_and_wraps() {
    register_builtin_node_types();

    const NodeType* phase = find_node_type("Phase");
    CHECK(phase != nullptr);

    Graph g;
    g.nodes.push_back(make_node(*phase, 41, "Phase A"));

    GraphBuildError err = g.bake();
    CHECK(err.code == GraphBuildErrorCode::None);
    CHECK(g.init_pass());

    CHECK(std::get<Scalar>(g.find_node(41)->outputs[0].current) == 0.0f);
    CHECK(std::get<Scalar>(g.find_node(41)->state["phase"]) == 0.0f);

    CHECK(g.tick(0.5f));
    CHECK(std::get<Scalar>(g.find_node(41)->outputs[0].current) == 0.25f);
    CHECK(std::get<Scalar>(g.find_node(41)->state["phase"]) == 0.25f);

    CHECK(g.tick(1.5f));
    CHECK(std::get<Scalar>(g.find_node(41)->outputs[0].current) == 0.0f);
    CHECK(std::get<Scalar>(g.find_node(41)->state["phase"]) == 0.0f);

    PASS("Phase node advances by dt/period and wraps cleanly");
    return 0;
}

int test_bpm_tap_node_outputs_bpm_and_period() {
    const NodeType* bpm_tap = find_node_type("BPMTap");
    CHECK(bpm_tap != nullptr);

    Node n = make_node(*bpm_tap, 60, "BPM Tap");
    n.state["bpm"] = SocketValue{Scalar{100.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 100.0f) < 0.0001f);
    CHECK(std::abs(std::get<Scalar>(n.outputs[1].current) - 0.6f) < 0.0001f);

    n.state["bpm"] = SocketValue{Scalar{500.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 300.0f) < 0.0001f);
    CHECK(std::abs(std::get<Scalar>(n.outputs[1].current) - 0.2f) < 0.0001f);

    PASS("BPM Tap node exposes clamped BPM and beat period outputs");
    return 0;
}

int test_multiply_node_multiplies_inputs() {
    const NodeType* multiply = find_node_type("Multiply");
    CHECK(multiply != nullptr);

    Node n = make_node(*multiply, 61, "Multiply");
    n.inputs[0].default_value = SocketValue{Scalar{2.0f}};
    n.inputs[0].current = SocketValue{Scalar{2.0f}};
    n.inputs[1].default_value = SocketValue{Scalar{0.5f}};
    n.inputs[1].current = SocketValue{Scalar{0.5f}};

    n.type->evaluate(n, 0.0f, 0.0f, false);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 1.0f);

    PASS("Multiply node multiplies scalar inputs");
    return 0;
}

int test_probe_coordinate_nodes_read_sample_position() {
    const NodeType* probe_x = find_node_type("ProbeX");
    const NodeType* probe_y = find_node_type("ProbeY");
    const NodeType* probe_z = find_node_type("ProbeZ");
    CHECK(probe_x != nullptr);
    CHECK(probe_y != nullptr);
    CHECK(probe_z != nullptr);

    set_builtin_probe_position(Vec3{0.25f, 0.75f, 0.5f});

    Node x = make_node(*probe_x, 62, "ProbeX");
    Node y = make_node(*probe_y, 63, "ProbeY");
    Node z = make_node(*probe_z, 64, "ProbeZ");
    x.type->evaluate(x, 0.0f, 0.0f, true);
    y.type->evaluate(y, 0.0f, 0.0f, true);
    z.type->evaluate(z, 0.0f, 0.0f, true);

    CHECK(std::get<Scalar>(x.outputs[0].current) == 0.25f);
    CHECK(std::get<Scalar>(y.outputs[0].current) == 0.75f);
    CHECK(std::get<Scalar>(z.outputs[0].current) == 0.5f);

    PASS("Probe coordinate nodes read the current sample position in XYZ");
    return 0;
}

int test_project2d_node_projects_onto_normalized_axis() {
    const NodeType* project2d = find_node_type("Project2D");
    CHECK(project2d != nullptr);

    Node n = make_node(*project2d, 65, "Project2D");
    n.inputs[0].current = SocketValue{Scalar{0.36f}};
    n.inputs[1].current = SocketValue{Scalar{0.15f}};
    n.inputs[2].current = SocketValue{Scalar{3.0f}};
    n.inputs[3].current = SocketValue{Scalar{4.0f}};
    n.inputs[4].current = SocketValue{Scalar{0.1f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.436f) < 0.0001f);

    n.inputs[2].current = SocketValue{Scalar{0.0f}};
    n.inputs[3].current = SocketValue{Scalar{0.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.1f) < 0.0001f);

    PASS("Project2D node projects XY onto a normalized 2D axis");
    return 0;
}

int test_project3d_node_projects_onto_normalized_axis() {
    const NodeType* project3d = find_node_type("Project3D");
    CHECK(project3d != nullptr);

    Node n = make_node(*project3d, 66, "Project3D");
    n.inputs[0].current = SocketValue{Scalar{0.36f}};
    n.inputs[1].current = SocketValue{Scalar{0.15f}};
    n.inputs[2].current = SocketValue{Scalar{0.50f}};
    n.inputs[3].current = SocketValue{Scalar{3.0f}};
    n.inputs[4].current = SocketValue{Scalar{4.0f}};
    n.inputs[5].current = SocketValue{Scalar{12.0f}};
    n.inputs[6].current = SocketValue{Scalar{0.1f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.6907692f) < 0.0001f);

    n.inputs[3].current = SocketValue{Scalar{0.0f}};
    n.inputs[4].current = SocketValue{Scalar{0.0f}};
    n.inputs[5].current = SocketValue{Scalar{0.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.1f) < 0.0001f);

    PASS("Project3D node projects XYZ onto a normalized 3D axis");
    return 0;
}

int test_band_node_windows_wrapped_positions() {
    const NodeType* band = find_node_type("Band");
    CHECK(band != nullptr);

    Node n = make_node(*band, 67, "Band");
    n.inputs[0].current = SocketValue{Scalar{0.02f}};
    n.inputs[1].current = SocketValue{Scalar{0.98f}};
    n.inputs[2].current = SocketValue{Scalar{0.08f}};
    n.inputs[3].current = SocketValue{Scalar{0.12f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 1.0f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{0.08f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.5f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{0.40f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.0f) < 0.0001f);

    PASS("Band node shapes wrapped scalar positions into a softened window");
    return 0;
}

int test_sine_node_maps_phase_to_unit_interval() {
    const NodeType* sine = find_node_type("Sine");
    CHECK(sine != nullptr);

    Node n = make_node(*sine, 64, "Sine");
    n.inputs[0].default_value = SocketValue{Scalar{0.0f}};
    n.inputs[0].current = SocketValue{Scalar{0.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.5f);

    n.inputs[0].current = SocketValue{Scalar{0.25f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 1.0f) < 0.0001f);

    PASS("Sine node maps phase-like scalar inputs into 0..1 wave output");
    return 0;
}

int test_ramp_node_wraps_phase_like_signal() {
    const NodeType* ramp = find_node_type("Ramp");
    CHECK(ramp != nullptr);

    Node n = make_node(*ramp, 65, "Ramp");
    n.inputs[0].current = SocketValue{Scalar{0.25f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.25f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{1.35f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.35f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{-0.15f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.85f) < 0.0001f);

    PASS("Ramp node wraps phase-like scalar inputs into a linear 0..1 wave");
    return 0;
}

int test_mix_node_lerps_between_inputs() {
    const NodeType* mix = find_node_type("Mix");
    CHECK(mix != nullptr);

    Node n = make_node(*mix, 66, "Mix");
    n.inputs[0].current = SocketValue{Scalar{0.2f}};
    n.inputs[1].current = SocketValue{Scalar{1.0f}};
    n.inputs[2].current = SocketValue{Scalar{0.25f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.4f) < 0.0001f);

    n.inputs[2].current = SocketValue{Scalar{2.0f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 1.0f);

    PASS("Mix node interpolates and clamps the blend factor");
    return 0;
}

int test_time_offset_wraps_normalized_signal() {
    const NodeType* time_offset = find_node_type("TimeOffset");
    CHECK(time_offset != nullptr);

    Node n = make_node(*time_offset, 67, "TimeOffset");
    n.inputs[0].current = SocketValue{Scalar{0.9f}};
    n.inputs[1].current = SocketValue{Scalar{0.3f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.2f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{0.1f}};
    n.inputs[1].current = SocketValue{Scalar{-0.4f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.7f) < 0.0001f);

    PASS("TimeOffset node wraps normalized scalar signals");
    return 0;
}

int test_mix_vec3_lerps_between_inputs() {
    const NodeType* mix_vec3 = find_node_type("MixVec3");
    CHECK(mix_vec3 != nullptr);

    Node n = make_node(*mix_vec3, 68, "MixVec3");
    n.inputs[0].current = SocketValue{Vec3{1.0f, 1.0f, 1.0f}};
    n.inputs[1].current = SocketValue{Vec3{1.0f, 0.0f, 0.0f}};
    n.inputs[2].current = SocketValue{Scalar{0.25f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    const Vec3 mixed = std::get<Vec3>(n.outputs[0].current);
    CHECK(std::abs(mixed[0] - 1.0f) < 0.0001f);
    CHECK(std::abs(mixed[1] - 0.75f) < 0.0001f);
    CHECK(std::abs(mixed[2] - 0.75f) < 0.0001f);

    PASS("MixVec3 node lerps Vec3 inputs with scalar blend");
    return 0;
}

int test_clamp_node_limits_scalar_range() {
    const NodeType* clamp = find_node_type("Clamp");
    CHECK(clamp != nullptr);

    Node n = make_node(*clamp, 69, "Clamp");
    n.inputs[0].current = SocketValue{Scalar{1.4f}};
    n.inputs[1].current = SocketValue{Scalar{0.2f}};
    n.inputs[2].current = SocketValue{Scalar{0.8f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.8f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{-0.3f}};
    n.inputs[1].current = SocketValue{Scalar{0.9f}};
    n.inputs[2].current = SocketValue{Scalar{0.1f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.1f) < 0.0001f);

    PASS("Clamp node constrains scalar values and normalizes inverted bounds");
    return 0;
}

int test_spatial_mirror_folds_position_around_center() {
    const NodeType* spatial_mirror = find_node_type("SpatialMirror");
    CHECK(spatial_mirror != nullptr);

    Node n = make_node(*spatial_mirror, 70, "SpatialMirror");
    n.inputs[0].current = SocketValue{Scalar{0.25f}};
    n.inputs[1].current = SocketValue{Scalar{0.5f}};
    n.inputs[2].current = SocketValue{Scalar{0.5f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.5f) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{0.75f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - 0.5f) < 0.0001f);

    PASS("SpatialMirror node folds both halves onto one mirrored coordinate");
    return 0;
}

int test_decay_node_holds_peaks_and_decays_over_time() {
    const NodeType* decay = find_node_type("Decay");
    CHECK(decay != nullptr);

    Node n = make_node(*decay, 71, "Decay");
    n.inputs[0].current = SocketValue{Scalar{1.0f}};
    n.inputs[1].current = SocketValue{Scalar{0.5f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 1.0f);
    CHECK(std::get<Scalar>(n.state["prev_value"]) == 1.0f);

    n.inputs[0].current = SocketValue{Scalar{0.0f}};
    n.type->evaluate(n, 0.5f, 0.5f, false);
    CHECK(std::abs(std::get<Scalar>(n.outputs[0].current) - std::exp(-1.0f)) < 0.0001f);

    n.inputs[0].current = SocketValue{Scalar{0.8f}};
    n.type->evaluate(n, 0.1f, 0.6f, false);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.8f);

    PASS("Decay node preserves peaks and releases exponentially");
    return 0;
}

int test_output_dimmer_clamps_scalar_output() {
    const NodeType* output_dimmer = find_node_type("OutputDimmer");
    CHECK(output_dimmer != nullptr);

    Node n = make_node(*output_dimmer, 72, "OutputDimmer");
    n.inputs[0].current = SocketValue{Scalar{1.3f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 1.0f);

    n.inputs[0].current = SocketValue{Scalar{-0.25f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.0f);

    PASS("OutputDimmer node clamps scalar output into dimmer range");
    return 0;
}

int test_output_tilt_clamps_scalar_output() {
    const NodeType* output_tilt = find_node_type("OutputTilt");
    CHECK(output_tilt != nullptr);

    Node n = make_node(*output_tilt, 73, "OutputTilt");
    n.inputs[0].current = SocketValue{Scalar{1.2f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 1.0f);

    n.inputs[0].current = SocketValue{Scalar{-0.1f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.0f);

    PASS("OutputTilt node clamps scalar output into tilt range");
    return 0;
}

int test_spatial_fixture_driver_exposes_coupled_outputs() {
    const NodeType* driver = find_node_type("SpatialFixtureDriver");
    CHECK(driver != nullptr);

    Node n = make_node(*driver, 74, "SpatialFixtureDriver");
    n.inputs[0].current = SocketValue{Scalar{0.75f}};
    n.inputs[1].current = SocketValue{Scalar{1.4f}};
    n.inputs[2].current = SocketValue{Vec3{1.2f, -0.1f, 0.4f}};
    n.type->evaluate(n, 0.0f, 0.0f, true);
    CHECK(std::get<Scalar>(n.outputs[0].current) == 0.75f);
    CHECK(std::get<Scalar>(n.outputs[1].current) == 1.0f);
    const Vec3 color = std::get<Vec3>(n.outputs[2].current);
    CHECK(std::abs(color[0] - 1.0f) < 0.0001f);
    CHECK(std::abs(color[1] - 0.0f) < 0.0001f);
    CHECK(std::abs(color[2] - 0.4f) < 0.0001f);

    PASS("SpatialFixtureDriver exposes clamped dimmer, tilt, and color outputs");
    return 0;
}

int test_seeded_default_spatial_patch_drives_mirrored_bar_rig() {
    Graph graph;
    NodeId next_id = 1;
    ConnectionId next_connection_id = 1;
    DefaultSpatialPatchIds ids;
    CHECK(seed_default_spatial_patch(graph, next_id, next_connection_id, &ids));
    CHECK(ids.spatial_fixture_driver_node_id != 0);
    CHECK(ids.preview_node_id == ids.spatial_fixture_driver_node_id);
    CHECK(ids.preview_output_socket_index == 2);

    FixtureId next_fixture_id = 1;
    const std::vector<FixtureProbe> probes = make_default_preview_probes(next_fixture_id);
    CHECK(probes.size() == 10);

    const FixtureProbe* left_1 = find_fixture_probe(probes, "Bar L1");
    const FixtureProbe* right_1 = find_fixture_probe(probes, "Bar R1");
    const FixtureProbe* left_5 = find_fixture_probe(probes, "Bar L5");
    CHECK(left_1 != nullptr);
    CHECK(right_1 != nullptr);
    CHECK(left_5 != nullptr);

    const DriverOutputs left_5_initial = evaluate_driver_outputs_at_probe(
        graph, ids.spatial_fixture_driver_node_id, left_5->position, 0, 1.0f / 60.0f);
    const DriverOutputs left_1_now = evaluate_driver_outputs_at_probe(
        graph, ids.spatial_fixture_driver_node_id, left_1->position, 18, 1.0f / 60.0f);
    const DriverOutputs right_1_now = evaluate_driver_outputs_at_probe(
        graph, ids.spatial_fixture_driver_node_id, right_1->position, 18, 1.0f / 60.0f);
    const DriverOutputs left_5_now = evaluate_driver_outputs_at_probe(
        graph, ids.spatial_fixture_driver_node_id, left_5->position, 18, 1.0f / 60.0f);

    CHECK(std::abs(left_1_now.dimmer - right_1_now.dimmer) < 0.0001f);
    CHECK(std::abs(left_1_now.tilt - right_1_now.tilt) < 0.0001f);
    CHECK(color_distance(left_1_now.color, right_1_now.color) < 0.0001f);

    CHECK(std::abs(left_1_now.tilt - left_5_now.tilt) > 0.01f ||
          color_distance(left_1_now.color, left_5_now.color) > 0.05f);
    CHECK(std::abs(left_5_initial.tilt - left_5_now.tilt) > 0.01f ||
          color_distance(left_5_initial.color, left_5_now.color) > 0.05f);

    PASS("Seeded default spatial patch drives mirrored but time-varying bar samples");
    return 0;
}

// ----------------------------------------------------------------------------

int main() {
    int rc = 0;
    rc |= test_register_and_find();
    rc |= test_find_unknown_returns_null();
    rc |= test_make_node_populates_outputs();
    rc |= test_make_node_populates_state();
    rc |= test_evaluate_writes_output();
    rc |= test_vec3_through_variant();
    rc |= test_fixture_probe_trait_helpers();
    rc |= test_all_node_types();
    rc |= test_graph_propagates_connections_in_topological_order();
    rc |= test_graph_uses_default_input_when_disconnected();
    rc |= test_graph_rejects_cycles();
    rc |= test_graph_rejects_multiple_sources_to_one_input();
    rc |= test_graph_init_pass_does_not_advance_time_sensitive_state();
    rc |= test_phase_node_advances_and_wraps();
    rc |= test_bpm_tap_node_outputs_bpm_and_period();
    rc |= test_multiply_node_multiplies_inputs();
    rc |= test_probe_coordinate_nodes_read_sample_position();
    rc |= test_project2d_node_projects_onto_normalized_axis();
    rc |= test_project3d_node_projects_onto_normalized_axis();
    rc |= test_band_node_windows_wrapped_positions();
    rc |= test_sine_node_maps_phase_to_unit_interval();
    rc |= test_ramp_node_wraps_phase_like_signal();
    rc |= test_mix_node_lerps_between_inputs();
    rc |= test_time_offset_wraps_normalized_signal();
    rc |= test_mix_vec3_lerps_between_inputs();
    rc |= test_clamp_node_limits_scalar_range();
    rc |= test_spatial_mirror_folds_position_around_center();
    rc |= test_decay_node_holds_peaks_and_decays_over_time();
    rc |= test_output_dimmer_clamps_scalar_output();
    rc |= test_output_tilt_clamps_scalar_output();
    rc |= test_spatial_fixture_driver_exposes_coupled_outputs();
    rc |= test_seeded_default_spatial_patch_drives_mirrored_bar_rig();

    if (rc == 0) {
        std::cout << "\nAll substrate tests passed.\n";
    } else {
        std::cout << "\nSome substrate tests FAILED.\n";
    }
    return rc;
}
