# Experiment 05 – Connected Field Sampling

**Date:** 2026-07-19  
**Goal:** Prove that the same spatial function can drive both the background
field and the per‑fixture dimmer output, closing the “Fixtures as Pixels” loop.

## Result

Success. The window shows a moving grayscale sine wave with ten red fixture
points overlaid. The points now pulse in perfect sync with the wave beneath
them, because the fixture dimmer uses the same mathematical formula as the
field. This is the first experiment where the visualisation and the data
output are driven by a single, unified spatial function.

## What it proved

- The compute shader can evaluate a spatial function (sine over X + time)
  and use it for both the visual field and the per‑fixture output without
  duplicating work.
- The `GPUFixureOutput` struct correctly carries the synchronised dimmer
  value from GPU to CPU.
- The visualisation pipeline (field blit + point overlay) accurately
  reflects the underlying data: when a fixture is over a bright part of the
  wave, its point is bright; when over a dark part, the point is dark.
- The “Fixtures as Pixels” concept is now fully validated at the GPU level:
  a single shader can generate a spatial field, sample it at fixture
  positions, output structured fixture data, and display everything in real
  time.

## Design decisions

### Same formula, not a texture sample

The fixture dimmer does not literally sample the texture with `imageLoad`.
Instead, it recalculates the same sine formula at the fixture’s X coordinate.
This avoids the need for a memory barrier between the field write and the
fixture read within the same dispatch, and it’s actually more accurate
(no pixelation from the finite texture resolution). In a real effect with
more complex math, the shader would evaluate the effect function once per
fixture, not sample an intermediate texture.

### Red points, not HSL‑coloured

As in Experiment 04, the points are pure red with brightness from dimmer.
The HSL data is still generated (hue=0, full saturation, lightness=dimmer)
and stored in the SSBO for future profile mapping, but the visualisation
keeps colour simple to make the synchronisation obvious.

## Observations

- The grayscale sine wave appears to have a wider “white” region than
  “black”. This is an artifact of the display’s sRGB gamma curve and human
  lightness perception, not a bug in the data. The underlying values are a
  correct linear gradient. The final busking surface will apply gamma
  correction or use sRGB textures for perceptual uniformity.

## Next steps

- Implement a real lighting effect (the diagonal red sweep) in a compute
  shader, using the same pipeline to visualise it.
- Begin designing the substrate‑to‑shader compiler that will generate such
  shaders automatically from a node graph.
