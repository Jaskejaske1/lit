# lit

A node-graph-based lighting control engine. **Surface** for live busking, **Substrate** for building effects. Bake-at-boundary topology, mathematical compositing in 4D space (X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete.  
- **Phase 1** (substrate core) — live.  
  The node graph engine, type registry, spatial math, and fixture probe model are implemented and tested.  
- **Phase 2+** (baked shader runtime, visual field preview, busking surface) — future research.

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

The only binary produced is `test_substrate`, which validates the substrate library.

Useful overrides:

```bash
LIT_BUILD_PROFILE=release nix-shell
LIT_BUILD_DIR=/tmp/lit-debug nix-shell
```

## Personal

Research project. No deadlines, no customers. The kid who used to think in lightshows is still here — the programmer is the tool building the door.
