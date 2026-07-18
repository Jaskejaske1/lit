// experiment_02 — Fixture sampling from a compute shader field
//
// Builds on Experiment 01: same moving sine field, but now the compute shader
// also samples the field at a set of virtual fixture positions and writes the
// results into a Shader Storage Buffer Object (SSBO). The CPU reads the SSBO
// and prints the sampled values to the terminal every frame.
//
// Proves: GPU-generated spatial field → sample at fixture positions → CPU readback.
// This is the core "Fixtures as Pixels" loop.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/gl3w.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr int kTexWidth  = 512;
constexpr int kTexHeight = 512;
constexpr int kNumFixtures = 10;   // matches the default Bar probes

// ---------------------------------------------------------------------------
// Compute shader source
// ---------------------------------------------------------------------------
static const char* kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

// Output field (writeonly – only used for display)
layout(binding = 0, r32f) uniform writeonly image2D u_field;

// Fixture output buffer
layout(std430, binding = 1) buffer u_fixture_output {
    float fixture_values[];
};

uniform vec2 u_fixture_uv[10];
uniform float u_time;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    float nx = float(pixel.x) / float(imageSize(u_field).x);
    float value = sin((nx * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;
    imageStore(u_field, pixel, vec4(value, 0.0, 0.0, 0.0));

    // Sample fixtures: recompute the field at each fixture UV.
    // Only one thread per work group does this, to avoid redundant writes.
    if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
        for (int i = 0; i < 10; i++) {
            float u = u_fixture_uv[i].x;
            // Use the same formula as the field: sin((u * 10.0 + u_time) * PI) * 0.5 + 0.5
            float sampled = sin((u * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;
            fixture_values[i] = sampled;
        }
    }
}
)glsl";
// ---------------------------------------------------------------------------
// Helper: check OpenGL errors
// ---------------------------------------------------------------------------
static void checkGlError(const char* where) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "GL error after %s: 0x%04X\n", where, err);
    }
}

// ---------------------------------------------------------------------------
int main() {
    // 1. SDL3 + OpenGL 4.6 core context
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Experiment 02 — Fixture Sampling",
                                           800, 600,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    if (gl3wInit() != 0) {
        fprintf(stderr, "gl3wInit failed\n");
        return 1;
    }

    // 2. Compile the compute shader
    GLuint compShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compShader, 1, &kComputeShaderSrc, nullptr);
    glCompileShader(compShader);
    GLint ok;
    glGetShaderiv(compShader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetShaderInfoLog(compShader, 512, nullptr, info);
        fprintf(stderr, "Compute shader compile error: %s\n", info);
        return 1;
    }
    GLuint compProgram = glCreateProgram();
    glAttachShader(compProgram, compShader);
    glLinkProgram(compProgram);
    glGetProgramiv(compProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetProgramInfoLog(compProgram, 512, nullptr, info);
        fprintf(stderr, "Compute program link error: %s\n", info);
        return 1;
    }
    glDeleteShader(compShader);

    // 3. Create the field texture (same as before)
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kTexWidth, kTexHeight, 0,
                 GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 4. Create the FBO for blitting
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
        return 1;
    }

    // 5. Create the SSBO for fixture output values
    GLuint fixtureSSBO;
    glGenBuffers(1, &fixtureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kNumFixtures * sizeof(float), nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, fixtureSSBO);

    // 6. Define fixture positions (normalized UV coordinates, matching the
    //    default Bar L/R probe layout from the substrate)
    float fixtureUVs[kNumFixtures * 2] = {
        // Bar L1..L5 (left side)
        0.32f, 0.15f,
        0.28f, 0.30f,
        0.24f, 0.45f,
        0.20f, 0.60f,
        0.16f, 0.75f,
        // Bar R1..R5 (right side, mirrored)
        0.68f, 0.15f,
        0.72f, 0.30f,
        0.76f, 0.45f,
        0.80f, 0.60f,
        0.84f, 0.75f,
    };

    // 7. Get uniform location for fixture UV array
    GLint fixtureUVLoc = glGetUniformLocation(compProgram, "u_fixture_uv[0]");
    if (fixtureUVLoc == -1) {
        fprintf(stderr, "Could not find u_fixture_uv uniform\n");
        return 1;
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Fixture sampling ready. Output values will appear below:\n");

    // 8. Main loop
    float time_val = 0.0f;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }

        // --- Compute pass ---
        glUseProgram(compProgram);

        // Bind field texture as image unit 0
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        // Upload time
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);

        // Upload fixture positions as uniform array
        glUniform2fv(fixtureUVLoc, kNumFixtures, fixtureUVs);

        // Dispatch compute
        glDispatchCompute((kTexWidth+15)/16, (kTexHeight+15)/16, 1);

        // Ensure all writes complete before we read the SSBO
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // --- Read fixture values back from SSBO ---
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
        float* mapped = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (mapped) {
            // Print the first 4 fixture values for quick monitoring
            printf("\r");
            for (int i = 0; i < 4; ++i) {
                printf("Fixt %d: %.3f  ", i, mapped[i]);
            }
            fflush(stdout);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        // --- Display the field (blit) ---
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, kTexWidth, kTexHeight,
                          0, 0, 800, 600,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        SDL_GL_SwapWindow(window);
        time_val += 0.016f;
    }

    // Cleanup
    glDeleteBuffers(1, &fixtureSSBO);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(compProgram);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
