#pragma once

#include <array>
#include <cstdint>
#include <variant>

// lit — substrate type system
//
// The substrate's typed value system. Structural, not semantic — the docs
// (data-model.txt:77-93) make this explicit: same structure (e.g. Vector3)
// can mean color, position, RGB depending on context. No "Color" type, no
// "Position" type. Conventions live in the consuming node.
//
// Future-proof: std::variant lets us add types (e.g. AudioBuffer) without
// changing anything that uses SocketValue.

namespace substrate {

// Numeric primitives. std::array is zero-dep, std::layout-compatible, and
// trivially swappable for glm::vec2/3/4 later if we want operator overloads.
using Scalar = float;
using Vec2   = std::array<float, 2>;
using Vec3   = std::array<float, 3>;
using Vec4   = std::array<float, 4>;
// AudioBuffer comes later — one enum entry + one alias, no other changes.

// Enumerated type tag. uint8_t to keep SocketSpec/StateKeySpec small.
enum class ValueType : uint8_t { Scalar, Vec2, Vec3, Vec4 };

// A typed value. Adding a new ValueType means: one alias above + one enum
// entry + this variant grows. Nothing in Socket, Node, NodeType, or the
// registry touches this.
using SocketValue = std::variant<Scalar, Vec2, Vec3, Vec4>;

}  // namespace substrate
