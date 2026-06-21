# lit

A node-graph-based lighting control engine. **Surface** for live busking (MA2-style executors), **Substrate** for building effects (TouchDesigner / Max/MSP-style). Bake-at-boundary topology, mathematical compositing in 4D space (X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete. SDL3 + ImGui + GL3W + OpenGL 4.6 GPU compute pipeline verified. Prototype code deleted; insights preserved.
- **Phase 1+** (real substrate) — design phase. Implementation begins by deriving from the docs below.

## Docs (read before any code change)

The docs are the source of truth. **If code and docs disagree, fix the docs first.**

- **[`docs/ideas.txt`](docs/ideas.txt)** — Why we chose what we chose. Design decisions, architecture, inspirations.
- **[`docs/data-model.txt`](docs/data-model.txt)** — Data structures (Node, Socket, Graph, Intention, Fixture, etc.) + engine-level decisions (frame rate, init pass, topological order).
- **[`docs/roadmap.txt`](docs/roadmap.txt)** — Build phases 0-6 with exit criteria for each.
- **[`docs/engineering-patterns.txt`](docs/engineering-patterns.txt)** — How-to patterns for performance-critical code: GPU compute → SSBO → readback, std430 layout, hybrid-GPU hints.

## Build

```bash
cmake -B cmake-build -S .
cmake --build cmake-build --config Debug
```

Currently produces only the static dependency libraries (SDL3, ImGui, GL3W via CMake FetchContent). No substrate executable yet — that's Phase 1+ work. See [build quickstart](docs/engineering-patterns.txt) (top of file) for more.

## Personal

Research project. No deadlines, no customers. The kid who used to think in lightshows is still here — the programmer is the tool building the door.
