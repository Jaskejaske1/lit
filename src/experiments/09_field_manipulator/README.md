# Experiment 09 – Raylib Field Manipulator

**Date:** 2026-08-14  
**Goal:** Replace the raw SDL/OpenGL viewport with Raylib and create a stable
3D field manipulator that proves the declarative field engine can be controlled
live.

## Result

Success as a prototype.

The experiment opens a Raylib window with:

- orbital camera
- dark viewport
- floor grid and RGB axes
- 24-fixture festival-style rig (back truss, front floor, side booms)
- sparse point cloud representing a scalar field
- fire-like heat colour ramp
- manual field controls

The operator can move an energy source through the stage with keys, change its
radius and intensity, switch between radial and vertical sweep modes, and toggle
automatic motion.

## What it proved

- The continuous spatial field can be manipulated live using a real viewport.
- Fixture colours respond naturally to their distance from the energy source.
- A declarative field engine can be driven from a small set of live parameters.
- Raylib provides a stable enough 3D viewport for further experiments.

## What it did not prove

- A usable busking interface.
- Realistic fixture types and rig layout.
- GPU performance or formal field compilation.
- The operator experience of a lighting console.

## Key insight

This experiment exposed the field engine directly as the UI. That made it feel
like a VJ tool rather than a lighting console. The correct architecture is:

- **Declarative field engine** underneath: continuous spatial fields, fixtures
  sample the field.
- **Imperative busking surface** on top: buttons, faders, flash states.
  “Blinders NOW”, “Strobes NOW”, “Sweep upstage”.

The operator must never think about moving a cloud. They must think about what
the rig should do, and the field engine must resolve that instantly.

This insight is documented in `docs/ideas.md` and `docs/roadmap.md`.

## Controls

- `Z` / `S` = move source forward / backward
- `Q` / `D` = move source left / right
- `A` / `E` = move source down / up
- `T` / `G` = radius bigger / smaller
- `U` / `J` = intensity up / down
- `1` / `2` = radial / vertical sweep field
- `Space` = toggle automatic motion
- left drag = orbit
- middle drag = pan
- scroll = zoom
- `Esc` = quit

## Next step

Experiment 10 should introduce a realistic festival rig (trusses, floor package,
blinders, side booms) and a hardcoded set of imperative cue buttons that trigger
instant field states. That is the first step toward the Operation surface.