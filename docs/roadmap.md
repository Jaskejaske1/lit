# lit — Roadmap

## Current vision

Declarative spatial field engine. See `docs/manifesto.txt`.

Fixtures sample continuous 3D fields at their world positions and map the
results to their own traits.

## Hard constraints that apply to every phase

- Zero-latency live tweaks — any parameter change must reach the math with
  zero perceptible delay.
- Native processing speed — no interpreted layers, no shader recompilation on
  parameter change, no GC pauses, no allocation on the hot path.
- Parameter values live in shared memory / uniform buffers accessible to both
  CPU and GPU; CPU writes, GPU reads next frame.
- This shapes every architectural choice. If a feature can't be made zero-latency,
  it doesn't belong in the live path.

## Bake constraint

- Topology is baked at build-time. Runtime only varies parameters.
- Build-time can be slow (compilation takes seconds to minutes). Live-time is
  just uniform writes.
- Topology mutation never happens live. The substrate compiles a node graph into
  a runtime artifact once; that artifact runs unchanged during performance.
- Mirrors VST plugins, GPU shaders, FPGA synthesis.
- Bake is a spectrum, not binary. Start interpreted, measure, bake as needed.

## Program split

1. **Builder** — substrate editor + visual field preview. Dev-only.
   Compiles and bakes Intentions.
2. **Operation** — runtime Intentions + busking surface + MIDI/OSC + DMX
   output. Lives at the show. Lean.
3. **Visualizer** — spatial field preview + receives baked shaders from
   Builder. Optional during live.
4. **(External) Capture** — real fixture viz, GDTF-based. Used as-is.

## Compute target

- CPU evaluation is the DEFAULT for typical busking rigs.
- GPU compute is OPTIONAL for scale: pixel walls, large 3D rigs, complex spatial
  math. The shader-field experiments were GPU-first to validate the concept.
- Most shows can run on a modern CPU. Big shows get GPU acceleration.

## Bake when it matters

- Small rigs: interpreted evaluation, no bake needed.
- Large rigs: bake starts to matter.
- Per-Intention decision, not all-or-nothing.

## Phase 0 — Validate the toolchain

**Status:** complete.

- What got built: GPU compute to SSBO to CPU readback to ImGui visualization loop.
- What it proved: the foundation works (window, GL context, GPU math, data roundtrip).
- UI prototypes (`lit_view`, `lit_playground`) have been archived. They served
  their purpose.

## Phase 1 — Shader-field research

**Status:** baseline established through experiments 01–09; ongoing.

### What got built

- **Experiment 01** — compute shader sine field, GPU display.
- **Experiment 02** — fixture sampling from compute-shader field via SSBO readback.
- **Experiment 03** — multi-channel fixture output.
- **Experiment 04** — visual fixture overlay.
- **Experiment 05** — connected field sampling.
- **Experiment 06** — diagonal red sweep with stateful GPU decay.
- **Experiment 07** — true 3D scalar field slices.
- **Experiment 08** — 3D fixture preview with moving radial energy field.
- **Experiment 09** — Raylib field manipulator; revealed the need to separate the declarative engine from the imperative operator surface.

### What these proved

- GPU compute shaders can generate continuous spatial fields.
- Fixtures can sample those fields at their UV positions.
- Structured per-fixture output can be written to an SSBO and read back to CPU.
- The “Fixtures as Pixels” concept works end-to-end.
- Stateful nodes (decay) can be implemented as extra SSBO state buffers.
- The visual output is a first usable field-preview slice.

Next: realistic rig + imperative cue layer before substrate-to-shader work.

## Phase 2 — Substrate-to-shader compilation

**Status:** next.

### Exit criterion

A substrate `Graph` can be compiled into a GLSL compute shader that produces
identical output to the CPU-side evaluation for a simple effect.

### What gets built

- `GraphCompiler` class that traverses a baked graph and emits GLSL function
  calls for each node type.
- Tests that compare CPU and GPU output for small graphs (Phase→Sine→Output).
- Initial handling of stateful nodes via SSBO allocations.

This is the bake step made concrete. It turns the substrate from an interpreted
CPU library into a GPU code generator.

## Phase 3 — Busking surface (Operation)

### Exit criterion

A touch-friendly executable that loads a compiled shader, displays a 3D field
preview, and lets you tweak parameters via faders and buttons.

### What gets built

- UI: retained-mode, touch-first (e.g., Qt Quick or Slint), not immediate-mode.
- Executor grid, tap-tempo, master faders, symmetry controls.

This is the live performance binary.

## Phase 4 — DMX/Art-Net/sACN output

### Exit criterion

Real fixtures respond to the busking surface.

### What gets built

- Protocol output layer.
- Per-universe scheduling.
- Fixture profile mapping.

## Phase 5 — GDTF visualizer and polish

### What gets built

- Real fixture models in a 3D scene.
- Library management.
- MIDI/OSC input.
- Audio FFT.

## Current baseline (2026-08-13)

- Windows: Visual Studio 18 2026, CMake preset `windows-debug`.
- NixOS: `nix-shell`, `lit_configure`, `lit_build`, `lit_test`.
- `test_substrate` passes.
- Experiments 01–06 compile and run on Windows and NixOS.
- No UI library is currently used for the field preview; the experiments use
  raw SDL3 + OpenGL.
