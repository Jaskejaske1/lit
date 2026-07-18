# Experiment 02 – Fixture Sampling from a Compute Shader Field

**Date:** 2026-07-18  
**Goal:** Prove that a GPU-generated spatial field can be sampled at specific
fixture positions, with the results read back to the CPU every frame. This is
the core “Fixtures as Pixels” data loop.

## Result

Success. A compute shader generates a moving sine wave field (512×512, R32F).
The same shader recalculates the field value at 10 predefined fixture UV
coordinates and writes the results into a Shader Storage Buffer Object (SSBO).
The CPU maps the SSBO and prints the sampled values to the terminal each frame.
The values change smoothly with the wave, proving real‑time GPU sampling works.

## New concepts introduced

### Shader Storage Buffer Object (SSBO)

An SSBO is a raw GPU memory buffer that can be read or written by both the
compute shader and the CPU. Unlike a texture, it has no pixel format or
filtering — it’s just an array of bytes. We use it to hold an array of
`float` values (one per fixture).

- Created with `glGenBuffers` + `glBufferData` with `GL_SHADER_STORAGE_BUFFER`.
- Bound to a binding point with `glBindBufferBase`.
- Declared in GLSL as `layout(std430, binding = 1) buffer u_fixture_output { float fixture_values[]; };`
- The shader writes into `fixture_values[i]` like a normal array.
- The CPU reads it back with `glMapBuffer` after a memory barrier.

### `glMapBuffer` – reading GPU data from CPU

After the compute shader writes to the SSBO, we need to transfer the data
back to CPU memory. `glMapBuffer` returns a pointer to a mapped region of the
buffer that the CPU can read. It’s a zero‑copy operation on many drivers
(the pointer may point directly into GPU‑accessible memory).

We call:
```cpp
float* mapped = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
// read mapped[i] ...
glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
```

The memory barrier (`GL_SHADER_STORAGE_BARRIER_BIT`) before mapping ensures
that all GPU writes are visible to the CPU.

### Uniform arrays – uploading fixture positions

We need to tell the shader where the fixtures are. We upload the UV
coordinates as a uniform array:

```cpp
GLint loc = glGetUniformLocation(program, "u_fixture_uv[0]");
glUniform2fv(loc, kNumFixtures, fixtureUVs);
```

The shader receives them as `uniform vec2 u_fixture_uv[10];`. This is
simple but has a fixed size limit (the array size must be known at compile
time). For a real rig with hundreds of fixtures, we’d use a second SSBO
for the positions.

### Separation of visualization and sampling

The compute shader both generates the field (for visual display) and samples
it at fixture positions (for data output). These two tasks are independent:
- The field is written to a texture and displayed via `glBlitFramebuffer`
  (smoothed with `GL_LINEAR` at 512×512 resolution).
- The fixture values are recalculated directly from the same mathematical
  formula, not from the texture. This avoids the “read from writeonly
  texture” GLSL limitation and is actually more accurate (no pixelation).

This separation means the field resolution and the fixture sampling
precision are independent. We can increase one without affecting the other.

## What this proves

- A compute shader can produce both a visual field and per‑fixture output
  in a single dispatch.
- SSBOs provide a fast, simple path for GPU→CPU data transfer.
- Uniform arrays work for small numbers of fixtures (≤100 or so).
- The CPU can receive updated fixture values every frame with minimal
  latency (one `glMemoryBarrier` + `glMapBuffer`).
- The “Fixtures as Pixels” concept is fully validated at the GPU level.

## Next steps

Experiment 03 will replace the hardcoded sine formula with a more complex
spatial effect (diagonal sweep with decay) written entirely in GLSL. This
will prove that the GPU can run a full lighting effect without a node graph
at runtime — a stepping stone to substrate‑to‑shader compilation.
