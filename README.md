# lit

A declarative spatial field engine for stage lighting.

Lighting should be described as what space does, not as a list of fixture commands. lit models the stage as a continuous 3D field over space and time. Fixtures are probes that sample that field at their world positions and convert the result into their own traits: intensity, color, pan/tilt, beam shape, and so on.

This is a field-theory approach to lighting: the designer composes mathematical relationships in space, and the engine resolves the output for any rig that exists in that space.

## The core idea

### 1. Space

The stage is a continuous coordinate system. Fixtures, beams, and scenic elements live in that same space. The engine thinks in positions and orientations, not in DMX addresses or fixture IDs.

### 2. Fields

A field is a function that answers: “What is happening at this point in space, at this moment in time?”

- Scalar fields: intensity, zoom, value
- Vector fields: direction, flow, orientation
- Target fields: points in space for look-at behavior

These fields can be composed, transformed, mirrored, masked, swept, and mixed mathematically. Symmetry is a field operation, not a cue duplication trick.

### 3. Fixtures as probes

A fixture is not an effect owner. It is a probe in the field.

Every frame, the fixture asks:

- What is the intensity at my position?
- What is the direction at my position?
- What is the color at my position?

The engine returns semantic values, and the fixture profile translates those values into the physical output it can actually produce.

## The system model

### Substrate

The Substrate is the authoring layer: a procedural node graph for building field logic. It is the data model and evaluation engine for the underlying math.

- typed sockets and connections
- graph evaluation in dependency order
- parameterized live values with zero-latency assumptions
- build-time topology and runtime parameter tweaks

### Surface

The Surface is the busking layer: the simple, touch-friendly front end used in performance.

- executors and intentions
- parameter faders and controls
- fast operator workflow
- access to the underlying substrate when needed

### Visual field preview

A preview of the field itself, not just a list of fixtures. This makes the spatial math visible before it is mapped into DMX or other protocols.

### Real fixture visualizer

Optional validation using real fixture models. This helps confirm that what the field preview shows is what the rig will actually do.

## Why this matters

- The rig is replaceable. The field remains the same.
- Symmetry is a coordinate transform, not a duplicated cue.
- Color and intensity exist as spatial properties, not per-fixture palettes.
- Effects are emergent: a circle, a chase, or a sweep are just field patterns.
- The designer composes space, not commands.

## Current status

### Phase 0 — toolchain validation

Complete.

- GPU compute to SSBO to CPU readback flow is working.
- SDL3 + OpenGL foundations are validated.
- UI prototypes were archived after their purpose was served.

### Phase 1 — shader-field research

Baseline established through experiments 01–06; ongoing.

These experiments proved that:

- GPU compute shaders can generate continuous fields
- fixtures can sample those fields at their world positions
- structured fixture output can be written and read back
- stateful behaviors such as decay are feasible in GPU data flow
- the “Fixtures as Pixels” model works end-to-end

Remaining research before Phase 2: true 3D fields, vector/target fields,
live parameter editing, and composed/runtime field operations.

### Phase 2 — substrate-to-shader compilation

Next.

The goal is to compile a substrate graph into a GLSL compute shader and validate that the GPU output matches the CPU-side evaluation for simple effects.

### Phase 3+ — busking surface and output protocols

Planned.

This includes the live runtime, touch-first surface, and DMX/Art-Net/sACN output layers.

## Build system

The project is organized around CMake and a shared substrate library.

### Windows

Requires Visual Studio 2026 with the Desktop development with C++ workload.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Executables land in:

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

## Project layout

- `src/substrate/` — core node graph and CPU-side substrate logic
- `src/experiments/` — GPU shader-field experiments
- `tests/` — validation tests for the substrate
- `docs/` — design and architecture documents
- `archive/` — earlier prototype UI work

## Binaries

- `test_substrate` — validates the substrate library
- `experiment_01` — compute shader sine field
- `experiment_02` — fixture sampling from a compute-shader field
- `experiment_03` — multi-channel fixture output
- `experiment_04` — visual fixture overlay
- `experiment_05` — connected field sampling
- `experiment_06` — diagonal red sweep with stateful GPU decay
- `experiment_07` — true 3D scalar field sampled by fixtures

Run an experiment from its build output directory, for example:

Windows:

```powershell
.\cmake-build\windows\bin\Debug\experiment_06.exe
```

NixOS:

```bash
./cmake-build/linux/bin/experiment_06
```

## Documentation

The current design direction is captured here:

- [docs/manifesto.md](docs/manifesto.md) — the field-theory vision
- [docs/ideas.md](docs/ideas.md) — architecture, inspirations, and constraints
- [docs/data-model.md](docs/data-model.md) — substrate data structures and rules
- [docs/roadmap.md](docs/roadmap.md) — build phases and exit criteria
- [docs/engineering-patterns.md](docs/engineering-patterns.md) — runtime and performance patterns

## Research direction

lit is not a node-graph editor, a shader toolkit, or a replacement for an existing console workflow. It is a research engine for a different model of lighting: declarative, spatial, and field-based.

The goal is not to imitate the current industry model exactly. The goal is to build a tool that treats light as a continuous mathematical environment and lets the rig behave as a system that samples it.

This is a research project with no customer deadlines. The programmer is the tool building the door.
