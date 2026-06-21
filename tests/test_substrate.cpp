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
#include <cstdlib>
#include <iostream>

#include <substrate/substrate.h>

using namespace substrate;

// ----------------------------------------------------------------------------
// Test types
// ----------------------------------------------------------------------------

// Constant: no inputs, one Scalar output, state["value"] is the produced value.
void constant_evaluate(Node& self, float /*dt*/, float /*elapsed*/, bool /*init_pass*/) {
    self.outputs[0].current = self.state.at("value");
}

void register_constant() {
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

// ConstantVec3: same idea but Vec3 — proves the variant carries vector types.
void constant_vec3_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = self.state.at("value");
}

void register_constant_vec3() {
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
    register_constant();
    register_constant_vec3();

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

int test_all_node_types() {
    const auto& all = all_node_types();
    CHECK(all.size() == 2);
    CHECK(all.count("Constant") == 1);
    CHECK(all.count("ConstantVec3") == 1);

    PASS("all_node_types() enumerates registry");
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
    rc |= test_all_node_types();

    if (rc == 0) {
        std::cout << "\nAll substrate tests passed.\n";
    } else {
        std::cout << "\nSome substrate tests FAILED.\n";
    }
    return rc;
}
