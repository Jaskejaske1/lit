# lit

A node-graph-based lighting control engine. **Surface** for live busking (MA2-style executors), **Substrate** for building effects (TouchDesigner / Max/MSP-style). Bake-at-boundary topology, mathematical compositing in 4D space (X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete. SDL3 + ImGui + GL3W + OpenGL 4.6 GPU compute pipeline verified. Prototype code deleted; insights preserved.
- **Phase 1.a** (substrate prototype baseline) — live. Registry, node instances, graph bake/topo order, init pass, and a real `Phase` generator now run on NixOS inside `lit_view`.
- **Phase 1.b** (first spatial field preview slice) — live. `lit_view` now seeds a small animated scalar field, lets you wire scalar nodes together, and samples the result over a 2D probe grid.
- **Phase 1+** (real effect-building substrate) — still ahead. No baked runtime artifact, no packaged Intentions, and no true 3D field visualizer yet.

## Docs (read before any code change)

The docs are the source of truth. **If code and docs disagree, fix the docs first.**

- **[`docs/ideas.txt`](docs/ideas.txt)** — Why we chose what we chose. Design decisions, architecture, inspirations.
- **[`docs/data-model.txt`](docs/data-model.txt)** — Data structures (Node, Socket, Graph, Intention, Fixture, etc.) + engine-level decisions (frame rate, init pass, topological order).
- **[`docs/roadmap.txt`](docs/roadmap.txt)** — Build phases 0-6 with exit criteria for each.
- **[`docs/engineering-patterns.txt`](docs/engineering-patterns.txt)** — How-to patterns for performance-critical code: GPU compute → SSBO → readback, std430 layout, hybrid-GPU hints.

## Build

On NixOS, the canonical entrypoint is the project shell:

```bash
nix-shell
lit_configure
lit_build
lit_test
lit_view
```

The shell exports the runtime linker paths needed for SDL, OpenGL, and the
GUI prototype to start correctly on NixOS. The helper commands anchor
themselves to the repo root, so they still work if you `cd` around after
entering the shell.

Useful overrides:

```bash
LIT_BUILD_PROFILE=release nix-shell
LIT_BUILD_DIR=/tmp/lit-debug nix-shell
```

If you want the raw CMake path, this is equivalent:

```bash
cmake -S . -B cmake-build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build/linux
```

Current prototype baseline:

- `test_substrate` validates the substrate skeleton and registry behavior.
- `test_substrate` now also validates graph bake/topo order, init pass behavior, cycle rejection, and the built-in `Phase`, `Multiply`, `Sine`, `ProbeX`, and `ProbeY` nodes.
- `test_substrate` now also covers the first test-case-oriented modifiers: `Mix`, `TimeOffset`, `SpatialMirror`, and `Decay`.
- `test_substrate` now also covers the minimal fixture/probe helpers that back the Builder-side sample-point model.
- `lit_view` opens a debug console for the current substrate types, including a live `Phase` node whose output advances over time.
- `lit_view` also lets you tweak disconnected input sockets and node state live, so the current prototype is a real substrate workbench rather than a read-only inspector.
- `lit_view` now includes a minimal connection editor, so you can wire compatible outputs into inputs and exercise the real graph bake rules from inside the prototype UI.
- `lit_view` now includes a first field-preview panel: each sampled cell owns its own persistent graph state, so temporal nodes can evolve differently across space.
- The field preview still renders a user-controlled 2D `X,Y` heatmap, but it now does so with time history per probe instead of stateless re-sampling.
- `lit_view` can now overlay a simple default probe layout on top of that heatmap, which makes the preview feel closer to sampled fixture positions instead of a purely abstract field.
- Those overlay probes are now explicit named sample points in the workbench, with editable world-space positions and a selected live probe that drives the inspector-side sample position.
- `lit_view` now also exposes those sample points as concrete sampled outputs with IDs, positions, and scalar values, and those values now come from exact per-point persistent graph evaluation rather than nearest-cell heatmap lookup.
- Those workbench sample points now sit on top of a minimal substrate-side `FixtureProbe` + `FixtureTrait` model, which is the first small bridge toward the docs' fixture/probe layer.
- On startup, `lit_view` now seeds a more effect-like 2D patch built from `ProbeX`, `ProbeY`, `SpatialMirror`, `TimeOffset`, `Sine`, `Decay`, and `Mix`, so the workbench starts closer to the diagonal-sweep ideas in the docs.
- `lit_view` can now explicitly reset itself back to that diagonal-sweep baseline, and the seeded graph uses named node instances instead of generic type-number labels.

This is still a prototype baseline, not the real Phase 1 engine. See
[build quickstart](docs/engineering-patterns.txt) (top of file) for more.

## Personal

Research project. No deadlines, no customers. The kid who used to think in lightshows is still here — the programmer is the tool building the door.
