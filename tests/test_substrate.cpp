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
    CHECK(all.size() >= 12);
    CHECK(all.count("Constant") == 1);
    CHECK(all.count("ConstantVec3") == 1);
    CHECK(all.count("Mix") == 1);
    CHECK(all.count("TimeOffset") == 1);
    CHECK(all.count("SpatialMirror") == 1);
    CHECK(all.count("Decay") == 1);
    CHECK(all.count("Multiply") == 1);
    CHECK(all.count("Sine") == 1);
    CHECK(all.count("ProbeX") == 1);
    CHECK(all.count("ProbeY") == 1);

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
    CHECK(probe_x != nullptr);
    CHECK(probe_y != nullptr);

    set_builtin_probe_position(Vec2{0.25f, 0.75f});

    Node x = make_node(*probe_x, 62, "ProbeX");
    Node y = make_node(*probe_y, 63, "ProbeY");
    x.type->evaluate(x, 0.0f, 0.0f, true);
    y.type->evaluate(y, 0.0f, 0.0f, true);

    CHECK(std::get<Scalar>(x.outputs[0].current) == 0.25f);
    CHECK(std::get<Scalar>(y.outputs[0].current) == 0.75f);

    PASS("Probe coordinate nodes read the current sample position");
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

int test_mix_node_lerps_between_inputs() {
    const NodeType* mix = find_node_type("Mix");
    CHECK(mix != nullptr);

    Node n = make_node(*mix, 65, "Mix");
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

    Node n = make_node(*time_offset, 66, "TimeOffset");
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

int test_spatial_mirror_folds_position_around_center() {
    const NodeType* spatial_mirror = find_node_type("SpatialMirror");
    CHECK(spatial_mirror != nullptr);

    Node n = make_node(*spatial_mirror, 67, "SpatialMirror");
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

    Node n = make_node(*decay, 68, "Decay");
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
    rc |= test_multiply_node_multiplies_inputs();
    rc |= test_probe_coordinate_nodes_read_sample_position();
    rc |= test_sine_node_maps_phase_to_unit_interval();
    rc |= test_mix_node_lerps_between_inputs();
    rc |= test_time_offset_wraps_normalized_signal();
    rc |= test_spatial_mirror_folds_position_around_center();
    rc |= test_decay_node_holds_peaks_and_decays_over_time();

    if (rc == 0) {
        std::cout << "\nAll substrate tests passed.\n";
    } else {
        std::cout << "\nSome substrate tests FAILED.\n";
    }
    return rc;
}
