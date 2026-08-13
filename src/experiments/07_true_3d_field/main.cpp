// experiment_07 — True 3D scalar field, visualised as three slices
//
// Validates that a scalar field can depend on X, Y, and Z.
//
// The compute shader evaluates the same continuous 3D field three ways:
//   - XY slice at a fixed Z
//   - XZ slice at a fixed Y
//   - YZ slice at a fixed X
//
// Each slice is rendered side by side. Fixture positions are still sampled,
// and their values are printed to the terminal for numeric validation.

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
constexpr int kTexWidth = 256;
constexpr int kTexHeight = 256;
constexpr int kNumFixtures = 10;

// ---------------------------------------------------------------------------
// Compute shader
// ---------------------------------------------------------------------------
static const char *kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba32f) uniform writeonly image2D u_xy;
layout(binding = 1, rgba32f) uniform writeonly image2D u_xz;
layout(binding = 2, rgba32f) uniform writeonly image2D u_yz;

layout(std430, binding = 3) buffer u_positions {
    vec3 positions[];
};

layout(std430, binding = 4) buffer u_values {
    float values[];
};

uniform float u_time;

const float sliceX = 0.5;
const float sliceY = 0.5;
const float sliceZ = 0.0;

// True 3D scalar field: depends on all three axes.
float field(vec3 p) {
    float v =
        sin(p.x * 6.2831 + u_time) *
        cos(p.y * 4.0) *
        sin(p.z * 3.0 + u_time * 0.7);

    return clamp(v * 0.5 + 0.5, 0.0, 1.0);
}

// Simple blue-to-red heat map.
vec3 heat(float t) {
    return mix(vec3(0.0, 0.0, 0.6), vec3(1.0, 0.0, 0.0), t);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= imageSize(u_xy).x || pixel.y >= imageSize(u_xy).y) return;

    float nx = float(pixel.x) / float(imageSize(u_xy).x);
    float ny = float(pixel.y) / float(imageSize(u_xy).y);

    // Three orthogonal slices through the same 3D field.
    float vXY = field(vec3(nx, ny, sliceZ));
    float vXZ = field(vec3(nx, sliceY, ny));
    float vYZ = field(vec3(sliceX, nx, ny));

    imageStore(u_xy, pixel, vec4(heat(vXY), 1.0));
    imageStore(u_xz, pixel, vec4(heat(vXZ), 1.0));
    imageStore(u_yz, pixel, vec4(heat(vYZ), 1.0));

    // One thread also samples the fixtures.
    if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
        for (int i = 0; i < values.length(); ++i) {
            values[i] = field(positions[i]);
        }
    }
}
)glsl";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        fprintf(stderr, "Shader compile error (%u): %s\n", type, info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createSliceTexture()
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kTexWidth, kTexHeight, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

static GLuint createFboForTexture(GLuint tex)
{
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "FBO incomplete\n");
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return fbo;
}

// ---------------------------------------------------------------------------
int main()
{
    // 1. SDL + OpenGL 4.6 core context
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow("Experiment 07 - True 3D Field Slices",
                                          960, 400,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
    {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    if (gl3wInit() != 0)
    {
        fprintf(stderr, "gl3wInit failed\n");
        return 1;
    }

    // 2. Compile compute shader
    GLuint compShader = compileShader(GL_COMPUTE_SHADER, kComputeShaderSrc);
    if (!compShader)
        return 1;

    GLuint compProgram = glCreateProgram();
    glAttachShader(compProgram, compShader);
    glLinkProgram(compProgram);

    GLint ok = 0;
    glGetProgramiv(compProgram, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char info[512];
        glGetProgramInfoLog(compProgram, 512, nullptr, info);
        fprintf(stderr, "Compute link error: %s\n", info);
        return 1;
    }
    glDeleteShader(compShader);

    // 3. Three slice textures + FBOs
    GLuint sliceTextures[3] = {
        createSliceTexture(),
        createSliceTexture(),
        createSliceTexture(),
    };

    GLuint sliceFbos[3] = {
        createFboForTexture(sliceTextures[0]),
        createFboForTexture(sliceTextures[1]),
        createFboForTexture(sliceTextures[2]),
    };

    // 4. Fixture positions and value SSBOs
    const float fixturePositions[kNumFixtures * 3] = {
        0.32f,
        0.15f,
        -0.05f,
        0.28f,
        0.30f,
        -0.025f,
        0.24f,
        0.45f,
        0.0f,
        0.20f,
        0.60f,
        0.025f,
        0.16f,
        0.75f,
        0.05f,
        0.68f,
        0.15f,
        -0.05f,
        0.72f,
        0.30f,
        -0.025f,
        0.76f,
        0.45f,
        0.0f,
        0.80f,
        0.60f,
        0.025f,
        0.84f,
        0.75f,
        0.05f,
    };

    GLuint posSSBO = 0;
    glGenBuffers(1, &posSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, posSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(fixturePositions),
                 fixturePositions, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, posSSBO);

    GLuint valSSBO = 0;
    glGenBuffers(1, &valSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, valSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, kNumFixtures * sizeof(float),
                 nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, valSSBO);

    printf("Experiment 07 - True 3D field slices ready.\n");
    printf("Sampled fixture values:\n");

    // 5. Main loop
    float time_val = 0.0f;
    double lastTick = (double)SDL_GetTicks() / 1000.0;
    bool running = true;

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }

        double now = (double)SDL_GetTicks() / 1000.0;
        float dt = (float)(now - lastTick);
        lastTick = now;
        if (dt <= 0.0f)
            dt = 0.016f;
        time_val += dt;

        // --- Compute pass ---
        glUseProgram(compProgram);
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);

        glBindImageTexture(0, sliceTextures[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, sliceTextures[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(2, sliceTextures[2], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(kTexWidth / 16, kTexHeight / 16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        // --- Read fixture values back for terminal validation ---
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, valSSBO);
        float *mapped = (float *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (mapped)
        {
            printf("\r");
            for (int i = 0; i < 4; ++i)
            {
                printf("Fixt %d: %.3f  ", i, mapped[i]);
            }
            fflush(stdout);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        // --- Display three slices side by side ---
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);

        glViewport(0, 0, winW, winH);
        glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        int third = winW / 3;

        for (int i = 0; i < 3; ++i)
        {
            int destX = i * third;
            int destW = (i == 2) ? (winW - destX) : third;

            glBindFramebuffer(GL_READ_FRAMEBUFFER, sliceFbos[i]);
            glBlitFramebuffer(0, 0, kTexWidth, kTexHeight,
                              destX, 0, destX + destW, winH,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    glDeleteProgram(compProgram);
    glDeleteBuffers(1, &valSSBO);
    glDeleteBuffers(1, &posSSBO);

    for (int i = 0; i < 3; ++i)
    {
        glDeleteFramebuffers(1, &sliceFbos[i]);
        glDeleteTextures(1, &sliceTextures[i]);
    }

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}