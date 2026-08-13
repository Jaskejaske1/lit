# lit — Ideas and Architecture

> Why we chose what we chose. Design decisions, architecture, inspirations.
>
> This document has been updated to reflect the field-engine direction in
> `docs/manifesto.md`. Older UI references are marked as superseded where
> applicable.

## The problem

- The translation tax between creative impulse and physical light.
- The opportunity is programming by intention on a spatial model, not by
  fixture ID on a channel model.
- The dual-state architecture: Surface = declarative Intentions, Substrate =
  procedural node graph, Escape Hatch = one keystroke cracks it open
  (Ableton + Max-for-Live model).
- 4D space (X, Y, Z + T), spatial bounding masks instead of groups,
  mathematical compositing instead of HTP/LTP.

## Core concept: Fixtures as Pixels

The stage is a continuous 2D/3D canvas. Effects are not per-fixture
calculations; they are shaders that render into a spatial texture. Each fixture
samples that texture at its world position to get its output values (intensity,
color, pan/tilt).

This inverts the traditional model:

- **Old:** CPU loops over fixtures, runs node graph per fixture → slow, scales
  badly.
- **New:** GPU renders the effect as a field. Sampling is O(1) per fixture,
  fully parallel.
- Symmetry, mirroring, and spatial transforms are just coordinate math in the
  shader.
- The substrate node graph becomes an authoring tool that compiles to GLSL at
  bake time.
- Live performance = upload uniforms to the shader. No graph evaluation at
  runtime.

This is the zero-latency, scalable foundation for a modern busking tool.

## System layers

### 1. Substrate

- Deep procedural node graph where effects are BUILT.
- TouchDesigner / Blender / Max/MSP-like.
- Subpatches: nodes that wrap other nodes.
- Live data flow visible inside the graph (wires carry the actual values, not
  just static connections).
- Not visible during live performance — sits underneath.

### 2. Surface

- Busking layer where effects are USED.
- Christian Jackson MA2 layout views, or Wolfmix-style — but powered by your
  own substrate modules.
- Familiar layout to operators (pages of executors, parameter knobs).
- Each executor = an Intention from your personal library.
- The escape hatch lives here: one keystroke cracks an executor open into its
  underlying substrate for live rewiring.

### 3. Visual field preview

- 3D previz of the spatial math BEFORE it hits fixtures.
- Shows the field, not fixtures — volumes, vectors, scalars depending on what
  the math is doing.
- Where you SEE what the substrate math actually does in space.
- Per-fixture sample points toggleable on top.

### 4. Real fixture visualizer

- Capture-like, GDTF-based.
- Real fixture models in a 3D scene.
- Confirms: "what you saw in the field preview is what this looks like on the
  actual fixtures."

## Build order

- Substrate first (everything else depends on it having real math to render).
- Surface second (needs substrate modules to wrap).
- Visual field preview third (so you can see what the substrate actually does
  during build).
- Real fixture visualizer last (final-stage confidence check, right before
  going to live DMX).

## Split between contexts

- **AT HOME (build):** substrate + visual field preview open. You build effects,
  see them in 3D, package them as modules.
- **LIVE (use):** surface open. You reach into your library, deploy effects,
  tweak by feel.
- The visual field preview and real fixture visualizer are optional on stage —
  useful for confidence, not required for busking.

## What each inspiration actually gives us

- **Wolfmix W1** — tactile FX engines (color/move/beam separated), busking-first
  feeling. Limited customization, doesn't scale to big rigs.
- **Max/MSP** — deep patching power. Audio-rate, generic, set-and-let-run. Not
  live-performance oriented — bad fit for festivals.
- **TouchDesigner** — spatial math over 3D canvas, GPU-accelerated. Imperative
  visual programming, kills live flow.
- **Ableton + Max4Live** — the build/use split (device surface, deep patch
  underneath). The INTERACTION MODEL we want.
- **MA2 / ChamSys** — industry output (DMX/Art-Net/sACN), fixture profiles.
  NOT the workflow.
- **Christian Jackson MA2 layout views** — the visual language of how operators
  actually structure their live surface.
- **Chataigne** — data routing between protocols (OSC, MIDI, DMX, ArtNet).
- **OSCPilot** — brutalist touch-first UI, fast response, zero internal logic.
- **Capture** — GDTF-based real fixture rendering. The reference for the real
  visualizer.

## Open questions

- Intention data model: what does a packaged substrate module actually look
  like on disk? State I/O, parameters, subpatch references, metadata.
- Substrate to surface communication: events, polling, shared memory?
- Node graph serialization format.
- Visual representation of nodes (icon, color, parameters visible inline?).
- How does the visual field preview know which axes/scale to show?
- Does the real fixture visualizer run on the same machine as the substrate,
  or separate?
- What's the minimum GDTF subset to make the real visualizer actually useful?
- What's the surface layout when you have 6 months of personal library — how
  do you find things?

## Personal motivation

- The kid who used to think in lightshows in his head is still there.
- The programmer is the tool that has to build the door.
- This is research. No deadlines. No customers. Just untangling an idea that
  nobody else has built.

## About the current codebase

- `src/substrate/` contains the CPU-side node graph engine: flat type registry,
  sockets, connections, graph evaluation, built-in node primitives.
- `src/experiments/` contains the GPU shader-field research program set.
- Experiments 01–06 validate the “Fixtures as Pixels” pipeline from a raw
  compute shader to a visual fixture overlay with stateful decay.
- The old `lit_view` and `lit_playground` UI prototypes are archived in
  `archive/`.

## Hard constraints

- Live tweaks of node inputs and combinations must be ZERO-LATENCY.
- Native processing speed — no interpreted layers, no recompile-on-tweak, no
  GC pauses, no shader rebuilds on parameter change.
- What this rules out: any approach where a parameter change requires
  compilation, allocation, or synchronization on the hot path.
- What this requires: parameters in shared memory / uniform buffers accessible
  to both CPU and GPU; CPU writes values, GPU reads next frame, no perceptible
  delay between operator input and math update.
- This is achievable — game engines do this all the time (data-oriented design,
  uniform buffers, per-frame update) — but it must be designed in from the
  start, not retrofitted.

## Working live inside nodes — resolved

- **Flavor B (hot-rewire)** is excluded by design. Topology never mutates live.
- **Flavor A (hot-tweak)** is the only live form of "inside nodes". Parameter
  values change, structure stays fixed.
- The Escape Hatch during a show = unambiguously Flavor A.
- Build-time is where any wiring/rearranging happens. Live-time is purely
  parameter tweaking.

## Bake constraint

- Topology is NEVER mutated live. Only parameter values change during
  performance.
- Why: this lets the substrate compile/bake node graphs into optimized runtime
  forms that run at native speed.
- Implication: build-time can be slow (compilation allowed, takes seconds to
  minutes), live-time is zero-latency (just uniform writes).
- Effect on the model: simpler, faster, harder to break. No concurrent graph
  mutation, no inconsistent topology states, no need to think about runtime
  graph versioning.
- Mirrors VST plugin architecture, GPU shader compilation, FPGA synthesis —
  fixed topology, varying parameters.
- **Bake is a spectrum, not binary.** You can bake partially:
  - Cache the compiled GLSL shader instead of recompiling.
  - Pre-allocate fixed-size buffers.
  - Pre-compute static parts of the math.
  - Full graph fusion into one optimized runtime (the heavy version).
- Pragmatic approach: start interpreted, measure, bake only as much as needed.
- Whether bake is needed depends on graph size and complexity. Small graphs may
  not need it at all.

## Program architecture

- **Shared substrate library** — the real work. Holds node graph engine, math
  primitives, fixture system, color math, OSC comms, serialization. All
  binaries link to it. STATIC library, lives in one repo, built once.
- **The library is the first deliverable.** Builder / Operation / Visualizer
  can't exist without the engine to drive them. They share everything —
  fixture types, color math, OSC, time signals. Don't write it three times.
- The system is three executables + one library + external:
  1. **Builder** — substrate editor + visual field preview. Dev-only.
     Compiles and bakes Intentions at build time.
  2. **Operation** — runtime Intentions + busking surface + MIDI/OSC +
     DMX/Art-Net/sACN output. Lives at the show. Lean by design.
  3. **Visualizer** — spatial field preview + receives baked shaders from
     Builder. Runs on beefy hardware with a real GPU. Optional during live.
  4. **(External) Capture** — real fixture viz, GDTF-based. Used as-is.
- All four (three + external) communicate over OSC (Open Sound Control).
  Standard in lighting + media worlds.
- Three binaries + one library = maximum flexibility. Each binary deploys
  independently based on the show's needs.

## Build system

- **CMake 3.20+** as the build tool.
- Dependencies are fetched via `FetchContent` at configure time
  (SDL3, GL3W, and previously ImGui). There is no `deps/` directory.
- Windows presets use Visual Studio 18 2026. Linux presets use Ninja.
- Output: `cmake-build/{windows,linux}/bin/` for executables,
  `cmake-build/{windows,linux}/lib/` for static libraries.
- VSCode tasks (`tasks.json`) wrap CMake configure + build. `cmake-tools`
  extension recommended.
- Future: add **vcpkg** if reproducible third-party dependency management is
  needed beyond `FetchContent`.

## Lighting output protocols

- **DMX512** — legacy serial protocol. ~44 Hz max per universe. The base of
  all professional lighting. Slowest but most universal.
- **Art-Net** — DMX over UDP (Ethernet). Multiple universes, higher throughput
  than serial DMX. Industry standard for modern rigs.
- **sACN (E1.31)** — DMX over multicast UDP. Similar to Art-Net, better
  multicast support, ACN family standard. Increasingly preferred over Art-Net
  in new installations.
- All three supported by the Operation binary. Selected per-fixture /
  per-universe in the spatial setup.
- These are the LAST layer — substrate math → per-fixture channel values →
  protocol packet → wire / ethernet.
- Why three protocols: existing venues have existing infrastructure. Operation
  needs to talk to whatever's there. No single protocol wins everywhere.
- Art-Net and sACN are network protocols — both run over standard Ethernet
  hardware. No special cards needed, just gigabit switches.

## Serialization

- **Live runtime format: FlatBuffers.** Zero-copy deserialization, random
  access by offset, schema evolution via field IDs. Operation loads FlatBuffers
  directly at showtime.
- **Builder save format: FlatBuffers.** Single canonical format. Builder writes
  FlatBuffers directly. Git stores binary blobs.
- **JSON export** is a Builder feature, not a default. Used when you want to
  share a patch via text or compare graphs outside Builder.
- **No JSON in the live path.** Operation loads FlatBuffers, period.
- **Bake step is graph → optimized graph, format stays FlatBuffers.**
- Alternatives considered and rejected: Protobuf (copies on read), Cap'n Proto
  (younger tooling), custom binary (maximum work), JSON-only (insufficient for
  performance).

## Compute target

- CPU evaluation is the DEFAULT. Modern multi-core CPUs handle spatial math
  over hundreds of fixtures easily.
- Reference: MA2 onPC does 4-8 universes of pixel mapping on a single CPU core
  with limited RAM. Same workload class.
- GPU compute is an OPTION for scale: pixel walls, 3D lighting rigs with
  hundreds of fixtures, complex spatial math over many positions.
- Operation PC scales with the show, not fixed-cost. A recent i3 + 16GB RAM is
  enough for typical busking shows. Pixel walls and 3D rigs are the trigger
  cases for needing GPU.
- The substrate can run interpreted (CPU) or compiled (GPU). Per-Intention
  decision based on graph size and complexity.
- This simplifies the architecture: most shows run on CPU only. No discrete GPU
  required.

## Modularity of a baked Intention

- A compiled shader per Intention absolutely accepts multiple inputs and changes
  live.
- Modularity doesn't disappear — it moves from "graph structure" (baked) to
  "named parameter slots" (live).
- Each parameter slot is a live-tweakable input.
- Think VST plugin: the synth is compiled, the knobs are live.
- Nothing is lost compared to a fully editable graph. Just a different boundary.

## Bake when it matters

- Small rigs: interpreted evaluation, no bake needed.
- Large rigs (pixel walls, 3D lighting rigs with hundreds of fixtures, complex
  spatial math): bake starts to matter.
- Per-Intention decision, not all-or-nothing.
- Pixel walls and 3D lighting rigs are the trigger cases for full bake.

## Time signal Generators

- **BPM tap** (pedal hardware) — beat clock + tempo. Default for music-driven
  effects.
- **Kick-in mic** (audio-reactive) — transient trigger when low frequencies
  spike.
- **LTC timecode** — absolute time. Rare for the user, mentioned for
  completeness.
- All three are node Generators. Different shape of time signal, different
  effect type.
- User confirmed: rarely uses timecode, BPM tap is the default.

## Continuous math + operator nudges

- The substrate runs continuous math. The sweep is always running, looping based
  on BPM.
- The operator doesn't TRIGGER the sweep — they shape its parameters live:
  intensity, speed, color, tilt range.
- Discrete cues vs continuous parameters. The substrate gives you continuous,
  the operator shapes continuously, no GO button.
- This is how you get reactivity to song structure without scripting it.

## Decay node primitive

- Exponential decay: `I(x, t) = exp(-dt / tau)` where `dt` is time since the
  sweep was at position `x`, `tau` is the time constant.
- One node primitive, multiple behaviors: pulse with trail, long persistence
  (phosphor effect), echo (output fed back to its own input at delay).
- User's "delay of parameters" intuition was right — exponential decay is
  exactly that.

## Test case: diagonal pixel bars red sweep + tilt sweep

- Rig: diagonal pixel bars, suspended in the middle of the stage. Default state:
  white (full intensity, white color). Symmetrical left and right.
- Effect: red color sweep moves from bottom to top of each bar with fade trail.
  Tilt sweeps simultaneously (coupled with the color sweep).
- Symmetry: bars are symmetric, sweep is mirrored on each half. The sweep
  intensity function is symmetric: `f(x) = f(-x)`.
- Tempo: sweep loops based on BPM (live tempo from a BPM tap pedal).
- Composition: at each fixture, color = lerp(white, red, sweep_intensity). When
  sweep is far, fixture is white. When sweep passes through, fixture shifts to
  red. Same pattern for tilt.
- Coupling: same sweep position drives both color and tilt (possibly with a time
  offset for a layered wave feel).

> This test case was implemented in Experiment 06. It now runs as a GPU compute
> shader with stateful decay.

## Starter node primitive set for the test case

- **Generators:** BPM Tap, Phase, Ramp
- **Modifiers:** Decay, Mix, Time Offset
- **Spatial:** Spatial Mirror
- **Output:** Spatial Fixture Driver

## Nested Intentions (subpatch mechanism)

- An Intention can be used as a node type inside another Intention's graph.
- Its exposed parameters become inputs to that node instance.
- The substrate treats the nested Intention as a single composite node.
- Escape Hatch reveals the nested Intention's full graph for live rewiring.
- This is how the library compounds: small reusable Intentions combine into
  bigger ones.
- Performance: bake at the Intention boundary. Once baked, a nested Intention is
  no longer nested at runtime — it's part of the compiled artifact. Zero nesting
  cost at runtime.

## Node type system

- Flat registry of node types, NOT class inheritance.
- Each type is a self-contained definition: type name + input schema + output
  schema + parameters + math.
- No "extends BPM Tap." Variants are new types.
- Mirrors TouchDesigner's OP system.
- Adding new node types stays trivial.
- **Type vs Name distinction:**
  - **Type** = classification in the registry. E.g., `BPM Tap`, `Phase`, `Decay`.
  - **Name** = instance label. `My BPM Tap #1`, `Slow BPM Tap`, `Phase A`.
  - Type tells you what math runs. Name tells you which instance you're looking at.

## Sockets and parameters are unified

- A Socket IS the parameter.
- Input Socket: name + data type + default value + optional connection.
- Output Socket: name + data type + produced value.
- UI: input sockets show editable value when disconnected, connected source
  value when connected. Output sockets show produced value.
- One concept, two roles based on direction.
- Show inline always: both inputs and outputs display their current value at all
  times.

## Connection model

- Connection is a SEPARATE OBJECT in the Graph, NOT a field on Socket.
- A Connection has: source Output Socket ID + destination Input Socket ID.
- Socket just has its current value, no Connection field.
- This separates concerns: Socket = data, Connection = wiring.

## Connection data conversion rules

- Connection is pure transport. No silent conversion.
- Allowed implicit conversions:
  - Same type to pass through unchanged.
  - int to float is implicit (lossless, every int fits in float).
- Forbidden implicit conversions (require explicit operator):
  - float to int requires Round / Floor / Ceil / Clamp.
  - Vector3 to Color requires explicit construction.
  - Color to Vector3 requires explicit extraction.
- Full table documented in the engine. Errors are explicit and named.

## Clone-and-modify UX

- Right-click any Intention in the library to Clone.
- It appears as a new editable copy.
- Crack it open (Escape Hatch) to modify the underlying graph.
- Save as new Intention.
- This is a primary way the library compounds over time.

## Presets

- **Type** = classification (`BPM Tap`, `Phase`, `Decay`).
- **Name** = instance label (`My BPM Tap #1`).
- **Preset** = pre-configured variant of a Type with default parameter values.
- Take a Type, configure its parameters, save as Preset.
- Presets are the Surface's entry point: pick a Preset, drop it, modify live,
  save as new Preset.
- Bridges substrate and surface.

## State (per-node runtime memory)

- Some Nodes need to remember things between frames.
- State is `{ name: value, ... }` — a dict of named fields.
- Type declares what state keys it needs.
- Node holds the actual values per key.
- Examples:
  - LFO state: `{ "phase": 0.5 }`
  - Decay state: `{ "prev_value": 0.7 }`

## Bypass flag

- Useful for A/B testing, temporary muting, debugging.
- Visual indicator on the node.
- Toggle in the inspector panel.
- Same concept as bypass in audio effects.

## Data model: Streams + Events

- Two fundamental concepts: Stream and Event.
- **Stream** — continuous data flow at some rate.
- **Event** — discrete event (no value).
- Streams carry structured values:
  - Scalar, Vector2, Vector3, Vector4, (Future) AudioBuffer.
- Color is Vector3 (HSL). Position is Vector3 (XYZ). They're conventions, not
  types.

## Stream rates

- **Frame-rate** (default) — updates every tick.
- **Event-rate** — updates only when value changes.

## Frame rate

- Default 60 Hz.
- Configurable per graph: 30 / 60 / 120 / 240 Hz.
- Decoupled from output rate.
- Time as float seconds, not frame counts.
- `dt` passed to nodes that need it.
- 60 Hz is enough for continuous math. 1000 Hz would be waste.

## Connection rules

- Same structure — connect.
- Different structure — conversion needed.
- Stream to Event — threshold detector.
- Event to Stream — trigger.
- Math nodes decide what to do with values. Type is structural, not semantic.

## Color workspace

- Color is Vector3 (H, S, L) — engine convention.
- Why HSL:
  - MA uses HSL.
  - Natural place for white at top and black at bottom.
  - More intuitive for "how bright" in lighting.
  - ChamSys has HSL wheel as color model.
- Future: CIE 1931, CCT for pro color science.
- Fixture-profile mapping handles white channel variations:
  - RGB: standard HSLtoRGB.
  - RGBW: low saturation to white channel wins.
  - CMY: inversion math.
  - No white channel: profile knows, ignores W.

## Editor UI design

> **Status:** superseded by the current field-engine direction. The final
> busking surface will use a retained-mode, touch-first UI. The node graph
> editor may still be built later, but not with ImGui.

Original notes:

- Drag-drop positioning.
- Snap-to-grid.
- Keyboard shortcut + search box.
- Inspector panel.
- Cable organizer / cable-snap feedback.
- Visual node design: compact, brutalist, data-dense.
- Reference: DaVinci Resolve Fusion, Max/MSP, TouchDesigner, Blender.

## Node data model

- Nodes are DATA, not "no-code programming".
- A node has inline parameter values OR input sockets, not both at once.
- Output sockets feed other nodes' inputs.
- Inline + input is the standard data-flow pattern.
