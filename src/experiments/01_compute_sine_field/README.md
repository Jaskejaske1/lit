# Experiment 01 – Compute Shader Sine Field

**Date:** 2026-07-18  
**Goal:** Prove that a GLSL compute shader can generate a moving scalar field
and display it on screen via SDL3 + OpenGL 4.6. No UI libraries, no substrate.

## Result

Success. A 256×256 single‑channel float texture is filled every frame by a
compute shader that writes a horizontally‑scrolling sine wave. The texture
is copied to the window framebuffer using `glBlitFramebuffer`. The pattern
moves smoothly at ~60 fps.

## How it works (per‑frame pipeline)

1. CPU updates `u_time` and uploads it as a uniform.
2. Compute shader dispatches 16×16 work groups to cover the entire texture.
   Each invocation calculates one pixel:
   - `nx` = x / width
   - `value = sin((nx * 10.0 + u_time) * π) * 0.5 + 0.5`
   - `imageStore(u_output, pixel, vec4(value, 0.0, 0.0, 0.0))`
3. `glMemoryBarrier` ensures all writes are visible.
4. `glBlitFramebuffer` copies the texture to the screen at window resolution
   (nearest‑neighbour filtering, so pixels appear blocky – that's intentional).
5. `SDL_GL_SwapWindow` presents the frame.

## Key design choices

### Why `glBlitFramebuffer` instead of a full‑screen quad?

The original version used a vertex + fragment shader to render a quad.
That approach required a VAO, correct vertex attribute setup, and careful
uniform / texture binding. A single mistake produced a black screen with
no OpenGL error. `glBlitFramebuffer` replaces all of that with one call
that copies a texture directly to the default framebuffer. It is simpler,
more robust, and perfectly sufficient for displaying a field preview.

This principle applies to the whole project: **use the simplest OpenGL
primitive that meets the need.** We can always add a full rendering
pipeline later if we need colour‑correct scaling or post‑processing.

### Why a compute shader and not a fragment shader?

A fragment shader runs as part of the normal drawing pipeline and writes
to the current framebuffer. A compute shader runs independently and can
write to any texture or buffer. For future experiments we need to:
- Sample the field at arbitrary fixture positions (not just pixel centres).
- Keep the field data for multiple frames (decay trails).
- Read the field back to the CPU or pass it to a DMX output layer.

A compute shader gives us complete control over where the output goes.
The display step is separate and trivial (a blit).

### Texture format: R32F

`GL_R32F` is a single‑channel 32‑bit floating‑point texture. It holds
the scalar field directly, with no normalisation to 0–255. This avoids
precision loss and matches the substrate's `Scalar` type.

## What this proves

- SDL3 + OpenGL 4.6 + GL3W toolchain works on NixOS for compute shaders.
- A compute shader can fill a texture in parallel (16×16 work groups).
- Uniforms (time) can be updated per frame with zero latency.
- The texture can be displayed without a complex rendering pipeline.
- The "shader as spatial field generator" concept is viable.

## Next steps

Experiment 02 will add a set of hardcoded fixture positions (like the
Bar L/R probes) and modify the compute shader to also output sampled
values at those positions into an SSBO. The CPU will read the SSBO back
to prove that "sample a texture at fixture positions" works on the GPU.
