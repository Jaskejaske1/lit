# lit

A node-graph-based lighting control engine. **Surface** for live busking (MA2-style executors), **Substrate** for building effects (TouchDesigner / Max/MSP-style). Bake-at-boundary topology, mathematical compositing in 4D space (X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete. SDL3 + ImGui + GL3W + OpenGL 4.6 GPU compute pipeline verified. Prototype code deleted; insights preserved.
- **Phase 1.a** (substrate prototype baseline) — live. Registry, node instances, graph bake/topo order, init pass, and a real `Phase` generator now run on NixOS inside `lit_view`.
- **Phase 1+** (real effect-building substrate) — still ahead. No visual graph editor, no real spatial preview, no packaged Intentions yet.

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
- `test_substrate` now also validates graph bake/topo order, init pass behavior, cycle rejection, and the built-in `Phase` node.
- `lit_view` opens a debug console for the current substrate types, including a live `Phase` node whose output advances over time.
- `lit_view` also lets you tweak disconnected input sockets and node state live, so the current prototype is a real substrate workbench rather than a read-only inspector.
- `lit_view` now includes a minimal connection editor, so you can wire compatible outputs into inputs and exercise the real graph bake rules from inside the prototype UI.

This is still a prototype baseline, not the real Phase 1 engine. See
[build quickstart](docs/engineering-patterns.txt) (top of file) for more.

## Personal

Research project. No deadlines, no customers. The kid who used to think in lightshows is still here — the programmer is the tool building the door.
