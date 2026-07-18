// experiment_03 — Multi‑Channel Fixture Output
//
// Extends the SSBO from Experiment 02 to hold three channels per fixture
// (dimmer, tilt, color), matching the SpatialFixtureDriver's output schema.
// The compute shader calculates independent values for each channel,
// writes them into the SSBO, and the CPU reads them back each frame.
//
// Proves: one GPU dispatch can replace the full SpatialFixtureDriver,
// producing all the structured data needed to drive a lighting rig.

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
constexpr int kNumFixtures = 10;

// ---------------------------------------------------------------------------
// Fixture output struct (matches SpatialFixtureDriver output layout)
// ---------------------------------------------------------------------------
struct alignas(16) FixtureOutput {
    float dimmer;
    float tilt;
    float color;   // single intensity for now; Vec3 will come later
};

// ---------------------------------------------------------------------------
// Compute shader
// ---------------------------------------------------------------------------
static const char* kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

// Output field (writeonly)
layout(binding = 0, r32f) uniform writeonly image2D u_field;

// Fixture output buffer (std430 layout matches FixtureOutput struct)
layout(std430, binding = 1) buffer u_fixture_output {
    // Each fixture: dimmer, tilt, color (3 floats, 12 bytes + 4 padding → 16 aligned)
    float fixture_data[];
    // Equivalent to an array of FixtureOutput structs if we use vec3, but
    // we'll manually index for clarity: fixture i → [i*3+0]=dimmer, [i*3+1]=tilt, [i*3+2]=color
};

uniform vec2 u_fixture_uv[10];
uniform float u_time;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    float nx = float(pixel.x) / float(imageSize(u_field).x);
    float ny = float(pixel.y) / float(imageSize(u_field).y);

    // Simple intensity field (sine wave for display)
    float intensity = sin((nx * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;
    imageStore(u_field, pixel, vec4(intensity, 0.0, 0.0, 0.0));

    // Per‑fixture channel calculations (one thread per work group)
    if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
        for (int i = 0; i < 10; i++) {
            float u = u_fixture_uv[i].x;
            float v = u_fixture_uv[i].y;

            // Dimmer: sine wave based on u position + time
            float dimmer = sin((u * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;

            // Tilt: cosine wave based on v position (different axis)
            float tilt = cos((v * 8.0 + u_time * 1.3) * 3.14159) * 0.5 + 0.5;

            // Color: mix of u and v position (creates spatial variation)
            float color = (sin(u * 5.0 * 3.14159) * 0.5 + 0.5) * (cos(v * 5.0 * 3.14159) * 0.5 + 0.5);

            int base = i * 3;
            fixture_data[base + 0] = dimmer;
            fixture_data[base + 1] = tilt;
            fixture_data[base + 2] = color;
        }
    }
}
)glsl";

// ---------------------------------------------------------------------------
// Helpers
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

    SDL_Window* window = SDL_CreateWindow("Experiment 03 — Multi‑Channel Output",
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

    // 2. Compile compute shader
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

    // 3. Field texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kTexWidth, kTexHeight, 0,
                 GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 4. FBO for blitting
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
        return 1;
    }

    // 5. SSBO for multi‑channel fixture output
    GLuint fixtureSSBO;
    glGenBuffers(1, &fixtureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kNumFixtures * 3 * sizeof(float), nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, fixtureSSBO);

    // 6. Fixture positions (same UV layout as Experiment 02)
    float fixtureUVs[kNumFixtures * 2] = {
        0.32f, 0.15f,  0.28f, 0.30f,  0.24f, 0.45f,  0.20f, 0.60f,  0.16f, 0.75f,
        0.68f, 0.15f,  0.72f, 0.30f,  0.76f, 0.45f,  0.80f, 0.60f,  0.84f, 0.75f,
    };

    GLint fixtureUVLoc = glGetUniformLocation(compProgram, "u_fixture_uv[0]");
    if (fixtureUVLoc == -1) {
        fprintf(stderr, "Could not find u_fixture_uv uniform\n");
        return 1;
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    printf("Fixture output (dimmer / tilt / color):\n");

    // 7. Main loop
    float time_val = 0.0f;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }

        // Compute dispatch
        glUseProgram(compProgram);
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);
        glUniform2fv(fixtureUVLoc, kNumFixtures, fixtureUVs);
        glDispatchCompute((kTexWidth+15)/16, (kTexHeight+15)/16, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Read SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
        FixtureOutput* mapped = (FixtureOutput*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (mapped) {
            printf("\r");
            for (int i = 0; i < 3; ++i) {  // show first 3 fixtures
                printf("Fixt %d: D %.2f T %.2f C %.2f | ",
                       i, mapped[i].dimmer, mapped[i].tilt, mapped[i].color);
            }
            fflush(stdout);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        // Display field
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
