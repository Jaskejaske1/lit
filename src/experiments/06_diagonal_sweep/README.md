# Experiment 06 – Diagonal Red Sweep with Stateful Decay

**Date:** 2026-07-19  
**Goal:** Implement the canonical diagonal red sweep effect entirely in a
compute shader, including stateful exponential decay, and visualise it
with the existing `GPUFixureOutput` + point overlay pipeline.

## Result

Success. The sweep runs smoothly at 60 Hz on the GPU. The background field
shows the mirrored bar coordinate and a smooth decay trail behind the
sweep head. Fixture points transition from white to red as the sweep
passes, with tilt values following the decay intensity. The effect math
mirrors the substrate’s `seed_default_spatial_patch`.

## Stateful decay implementation

The original hand‑written shader approximated decay statelessly, which
caused a hard cut‑off after half a cycle. This version adds two small
SSBOs to hold per‑pixel and per‑fixture decay state (`prev_value`).
Each frame the shader reads the previous decay, applies the exponential
formula `max(input, prev * exp(-dt/tau))`, and writes back the updated
state. This is exactly how the substrate’s CPU `Decay` node works.

**Key insight for Phase 2:** Stateful GPU nodes are just a buffer per
state variable. The substrate‑to‑shader compiler will allocate such
buffers automatically when it encounters nodes with `state_schema` entries.

## Parameters (hardcoded for this experiment)

| Parameter    | Value              |
| ------------ | ------------------ |
| BPM          | 100                |
| Beats/sweep  | 3                  |
| Sweep period | 1.8 s              |
| Band width   | 0.10               |
| Band feather | 0.10               |
| Decay τ      | 0.28 s             |
| Base tilt    | 0.30               |
| Peak tilt    | 0.82               |
| Bar axis     | (0.47, 0.88, 0.18) |

## What this proves

- A full lighting effect (spatial mirror, 3D projection, band windowing,
  exponential decay, colour mixing, tilt control) can run entirely on the
  GPU in a single compute dispatch.
- Stateful nodes are practical and cheap—each state variable adds a
  small SSBO with one read and one write per frame.
- The `GPUFixureOutput` struct (dimmer, pan, tilt, HSL) correctly
  captures all the channels the effect produces.
- The visualisation pipeline (field blit + point overlay) works for real
  effects, not just test patterns.
- This shader is the exact target output of the substrate‑to‑shader
  compiler.

## Next steps

Phase 2: design the `GraphCompiler` that traverses a baked substrate
graph and emits GLSL code equivalent to this hand‑written shader.
