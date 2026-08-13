# Experiment 08 – 3D Fixture Preview with a Moving Radial Energy Field

**Date:** 2026-08-13  
**Goal:** Create a meaningful 3D field preview where a moving radial energy
source drives fixture intensity/colour in true 3D space.

## Result

Success.

The experiment shows:

- 12 fixtures arranged in three columns and four heights, with real depth
  variation in Z.
- A moving radial energy source that travels through the stage volume.
- A sparse 3D point cloud that visualises the scalar field around the source.
- A floor reference grid for spatial orientation.
- An orbit camera for inspecting the scene in 3D.

Fixtures sample the same scalar field as the point cloud. When the energy
source moves near a fixture, that fixture becomes brighter and warmer.
Far fixtures stay dark and cool.

## Field definition

```glsl
vec3 source = vec3(
    0.5 + 0.22 * sin(u_time * 0.8),
    0.5 + 0.30 * sin(u_time * 0.5),
    0.05 + 0.15 * sin(u_time * 0.6)
);

float radius = 0.28;
float d2 = dot(p - source, p - source);
float v = exp(-d2 / (radius * radius));
```

This is a Gaussian falloff around a moving point in 3D. It is a simple but
meaningful lighting field: proximity to an energy source.

## Colour ramp

The scalar value is mapped through a fire-like ramp:

- low → dark blue
- mid → orange / yellow
- high → red

This improves visual contrast against the dark background and makes the field
readable.

## What this proved

- A field that depends on X, Y, and Z can be visualised in actual 3D.
- Fixture positions can exist meaningfully in 3D, not just as a flat top-down
  layout.
- A moving spatial energy source produces intuitive per-fixture behaviour.
- A simple orbit camera is sufficient for inspecting early 3D field previews.

## Known limitations

- The field cloud is sparse and point-based. No smooth volume rendering yet.
- The moving source is hardcoded; parameters are not yet exposed as uniforms.
- Orbit only, no pan. No keyboard shortcuts beyond Escape.
- Fixture colours are computed on the CPU from SSBO readback; future versions
  can compute colour entirely on the GPU.

## Next step

Experiment 09 should add live parameter tweaks for the energy source position,
radius, and colour mapping. This is the first step toward a busking-style
field manipulator.
