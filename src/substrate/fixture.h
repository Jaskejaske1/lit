#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "value.h"

// lit — minimal fixture/probe data model
//
// This is the first concrete bridge from abstract spatial fields toward the
// docs' "Fixture (The Probe)" concept. It stays intentionally small:
//   - position in space
//   - stable ID
//   - capability traits
//
// The real spatial operator and fixture driver still come later.

namespace substrate {

using FixtureId = uint64_t;

enum class FixtureTrait : uint8_t {
    Dimmer,
    ColorRGB,
    Pan,
    Tilt,
};

inline std::string_view fixture_trait_name(FixtureTrait trait) {
    switch (trait) {
        case FixtureTrait::Dimmer:   return "Dimmer";
        case FixtureTrait::ColorRGB: return "Color_RGB";
        case FixtureTrait::Pan:      return "Pan";
        case FixtureTrait::Tilt:     return "Tilt";
    }
    return "UnknownTrait";
}

struct FixtureProbe {
    FixtureId id = 0;
    std::string name;
    Vec3 position{0.0f, 0.0f, 0.0f};
    std::vector<FixtureTrait> traits;
};

inline bool fixture_has_trait(const FixtureProbe& probe, FixtureTrait trait) {
    return std::find(probe.traits.begin(), probe.traits.end(), trait) != probe.traits.end();
}

inline void fixture_set_trait(FixtureProbe& probe, FixtureTrait trait, bool enabled) {
    const auto it = std::find(probe.traits.begin(), probe.traits.end(), trait);
    if (enabled) {
        if (it == probe.traits.end()) {
            probe.traits.push_back(trait);
        }
        return;
    }

    if (it != probe.traits.end()) {
        probe.traits.erase(it);
    }
}

}  // namespace substrate
