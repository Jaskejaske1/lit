# AGENTS.md

## Purpose

This repository is a hobby research project and a learning vehicle for C++,
graphics, and lighting-engine design. Assistants should optimize for clarity,
experimentation, and preserving insight, not for premature production process.

## Repo Posture

- This is not a production system.
- Prototypes are allowed when they answer a concrete design or runtime question.
- Docs are the source of truth while the architecture is still forming.
- If code and docs disagree, update the docs first or in the same change.

## Development Priorities

- Keep both Windows and NixOS builds healthy.
- Preserve a working prototype baseline before adding new features.
- Prefer small, testable steps that answer one design question at a time.
- Avoid building later phases into Phase 1 code.

## Canonical Workflow

### Windows

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Run tests and experiments from:

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

Run tests and experiments from:

```text
cmake-build/linux/bin/
```

The Nix shell is responsible for the runtime linker environment needed by SDL,
OpenGL, and the GUI prototype on NixOS.

## Code Guidance

- Prefer simple data-oriented structures over deep inheritance.
- Keep the substrate library usable from tests and thin executables.
- Add comments when they preserve design intent, not to narrate syntax.
- Do not harden speculative abstractions before the docs settle them.

## Validation

Before calling a baseline healthy, verify:

1. `test_substrate` passes.
2. Experiments 01–06 compile and run.
3. The current platform’s build workflow succeeds.
4. No obsolete UI targets (`lit_view`, `lit_playground`) are referenced outside
   `archive/`.

## Editing Boundaries

- Do not delete or rewrite design docs casually; they are accumulated thinking.
- Do not treat prototype code as disposable unless the docs explicitly say the
  insight has been preserved elsewhere.
- If you introduce a new workflow assumption, document it in `README.md`.

Then commit everything:

```powershell
git add README.md AGENTS.md CMakeLists.txt CMakePresets.json
git commit -m "Update docs and CMake for Windows/NixOS support and experiments 01-06"
git push
```
