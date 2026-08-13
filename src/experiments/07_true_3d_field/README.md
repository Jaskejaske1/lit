# Experiment 07 – True 3D Scalar Field, Slice Visualisation

**Date:** 2026-08-13  
**Goal:** Prove that a scalar field can depend on **all three spatial axes**
(X, Y, Z), not just the 2D UV coordinates used in earlier experiments.

## Result

Success.

The compute shader defines a continuous field:

```glsl
float field(vec3 p) {
    float v =
        sin(p.x * 6.2831 + u_time) *
        cos(p.y * 4.0) *
        sin(p.z * 3.0 + u_time * 0.7);

    return clamp(v * 0.5 + 0.5, 0.0, 1.0);
}
```

This field is evaluated at every fixture’s true 3D world position. The sampled
values are written to an SSBO and printed to the terminal.

The visual output shows three orthogonal slices through the same field:

- **Left:** XY slice at fixed Z (top-down view)
- **Middle:** XZ slice at fixed Y (side view)
- **Right:** YZ slice at fixed X (front view)

All three slices move and differ, proving the field changes with Z as well as X
and Y.

## Why this matters

Earlier experiments sampled fields using normalized `(u, v)` coordinates and
treated Z as mostly decorative. This experiment confirms that fixtures with
different heights/depths will naturally sample different values from the same
spatial field.

This is a necessary foundation for:

- beam orientation from 3D vector fields
- height-dependent intensity / color behaviour
- true 3D field preview

## Limitations

The visualisation is still three **2D slices**, not a true 3D rendering. A real
3D viewport with orbit camera is the next step.

## Next step

Experiment 08 will render the field as a 3D point cloud with an orbit camera,
making the spatial structure directly visible.
