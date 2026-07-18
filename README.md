# lit

A node-graph-based lighting control engine. **Surface** for live busking, **Substrate** for building effects. Bake-at-boundary topology, mathematical compositing in 4D space (X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete.
- **Phase 1** (shader‑field research) — in progress.
  - The substrate core (node graph, registry, spatial math, fixture probes) is implemented and tested.
  - Experiments are validating the “Fixtures as Pixels” concept: GPU compute shaders that generate a spatial field, sample it at fixture positions, and stream results to the CPU. See `src/experiments/`.
- **Phase 2+** (substrate‑to‑shader compilation, busking surface, DMX output) — future.

## Docs

- **[`docs/ideas.txt`](docs/ideas.txt)** — Design decisions, architecture, inspirations.
- **[`docs/data-model.txt`](docs/data-model.txt)** — Data structures and engine rules.
- **[`docs/roadmap.txt`](docs/roadmap.txt)** — Build phases with exit criteria.
- **[`docs/engineering-patterns.txt`](docs/engineering-patterns.txt)** — Performance patterns.

## Build

On NixOS:

```bash
nix-shell
lit_configure
lit_build
lit_test
```

This produces:
- `test_substrate` – validates the substrate library.
- `experiment_01` – compute shader sine field with GPU display.
- `experiment_02` – fixture sampling from a compute shader field via SSBO readback.

Run experiments directly from the build directory:

```bash
./cmake-build/linux/bin/experiment_01
./cmake-build/linux/bin/experiment_02
```

Useful overrides:

```bash
LIT_BUILD_PROFILE=release nix-shell
LIT_BUILD_DIR=/tmp/lit-debug nix-shell
```

## Personal

Research project. No deadlines, no customers. The kid who used to think in lightshows is still here — the programmer is the tool building the door.
