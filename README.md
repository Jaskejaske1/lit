# lit

A declarative spatial field engine for stage lighting. Effects are continuous
functions over 3D space + time. Fixtures sample those fields at their world
positions and map the results to their own traits.

A node-graph Substrate is the authoring front-end. GPU compute shaders are the
runtime. Bake-at-boundary topology, mathematical compositing in 4D space
(X, Y, Z + T), zero-latency parameter tweaks.

## Status

- **Phase 0** (toolchain validation) — complete.
- **Phase 1** (shader-field research) — in progress.
  - The substrate core (node graph, registry, spatial math, fixture probes)
    is implemented and tested.
  - Experiments 01–06 validate the “Fixtures as Pixels” concept:
    GPU compute shaders generate continuous spatial fields, sample them at
    fixture positions, write structured fixture output to an SSBO, and display
    the result in a window.
- **Phase 2+** (substrate-to-shader compilation, busking surface, DMX output)
  — future.

## Docs

- **[`docs/manifesto.txt`](docs/manifesto.txt)** — The field-theory vision.
- **[`docs/ideas.txt`](docs/ideas.txt)** — Design decisions, architecture, inspirations.
- **[`docs/data-model.txt`](docs/data-model.txt)** — Data structures and engine rules.
- **[`docs/roadmap.txt`](docs/roadmap.txt)** — Build phases with exit criteria.
- **[`docs/engineering-patterns.txt`](docs/engineering-patterns.txt)** — Performance patterns.

## Build

### Windows

Requires Visual Studio 2026 with the **Desktop development with C++** workload.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Debug executables land in:

```text
cmake-build/windows/bin/Debug/
```

### NixOS

```bash
nix-shell
lit_configure
lit_build
lit_test
```

Executables land in:

```text
cmake-build/linux/bin/
```

Useful overrides:

```bash
LIT_BUILD_PROFILE=release nix-shell
LIT_BUILD_DIR=/tmp/lit-debug nix-shell
```

## Binaries

- `test_substrate` — validates the substrate library.
- `experiment_01` — compute shader sine field, GPU display.
- `experiment_02` — fixture sampling from a compute-shader field via SSBO readback.
- `experiment_03` — multi-channel fixture output.
- `experiment_04` — visual fixture overlay.
- `experiment_05` — connected field sampling.
- `experiment_06` — diagonal red sweep with stateful GPU decay.

Run an experiment from its build directory, for example:

Windows:

```powershell
.\cmake-build\windows\bin\Debug\experiment_06.exe
```

NixOS:

```bash
./cmake-build/linux/bin/experiment_06
```

## Personal

Research project. No deadlines, no customers. The kid who used to think in
lightshows is still here — the programmer is the tool building the door.
