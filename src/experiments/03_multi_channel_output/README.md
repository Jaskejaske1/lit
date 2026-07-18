# Experiment 03 – Multi‑Channel Fixture Output

**Date:** 2026-07-18  
**Goal:** Prove that a single GPU compute dispatch can produce structured,
multi‑channel fixture data (dimmer, tilt, color) — the same schema the
substrate’s `SpatialFixtureDriver` emits.

## Result

Success. The compute shader generates a moving intensity field (for display)
and simultaneously calculates independent dimmer, tilt, and colour values
for each of the 10 fixture probes. These values are written into a Shader
Storage Buffer Object (SSBO) with 3 floats per fixture (12 bytes + 4 padding,
16‑byte aligned). The CPU maps the SSBO each frame and prints the first
three fixtures’ data to the terminal.

## Key design decisions

### SSBO layout

The buffer holds `kNumFixtures * 3` floats, indexed as:

- `fixture_data[i * 3 + 0]` = dimmer
- `fixture_data[i * 3 + 1]` = tilt
- `fixture_data[i * 3 + 2]` = color

This flat layout avoids GLSL’s restrictions on arrays of user‑defined structs
in SSBOs, and is exactly what a substrate‑to‑shader compiler would emit for
the `SpatialFixtureDriver` output schema.

### C++ struct mirror

The CPU side reads the buffer as an array of `FixtureOutput`:

```cpp
struct alignas(16) FixtureOutput {
    float dimmer;
    float tilt;
    float color;
};
```

The `alignas(16)` ensures the struct matches the `std430` layout (each element
occupies 16 bytes, with 4 bytes of trailing padding). `glMapBuffer` returns
a pointer directly to this struct array — no deserialisation needed.

### Channel calculations are independent

Dimmer, tilt, and color are calculated from different combinations of fixture
UV coordinates and time. This demonstrates that a shader can drive multiple
fixture parameters with a single dispatch. In a real effect (e.g., the
diagonal red sweep), dimmer would be a constant, tilt would be a mix of
the sweep signal, and color would lerp between white and red.

## What this proves

- One GPU dispatch can replace the full `SpatialFixtureDriver` node:
  multiple channels, all fixtures, in parallel.
- SSBOs can hold structured data with correct alignment for CPU readback.
- The “Fixtures as Pixels” pipeline now produces output in the exact format
  the Operation binary would need to drive DMX.
- The path from substrate graph → shader output is now fully understood:
  the graph defines what channels to output; the shader writes them into
  the SSBO in the correct order.

## Next steps

Experiment 04 will add a minimal visual overlay: fixture points drawn on top
of the field texture, with brightness from the dimmer channel and colour from
the color channel. This will make the busking‑style rig preview visible
without adding a UI library.
