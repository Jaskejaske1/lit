// experiment_04 — Visual Fixture Overlay (clean, corrected)
//
// Builds on Experiment 03: the compute shader outputs the GPUFixureOutput struct
// (dimmer, pan, tilt, HSL) per fixture. For this experiment, visualisation uses
// a fixed red colour scaled by dimmer, ignoring HSL (HSL is in the struct for
// future profile mapping). The background field is now RGBA for correct grayscale.
//
// Proves: structured GPU output → visual overlay with brightness control.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/gl3w.h>

constexpr int kTexWidth  = 512;
constexpr int kTexHeight = 512;
constexpr int kNumFixtures = 10;

struct alignas(16) GPUFixureOutput {
    float dimmer;
    float pad0;
    float pan;
    float tilt;
    float hsl_h;
    float hsl_s;
    float hsl_l;
    float pad1;
};

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

// Compute shader: writes RGBA field + fixture output (HSL placeholder)
static const char* kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba32f) uniform writeonly image2D u_field;

layout(std430, binding = 1) buffer u_fixture_output {
    float fixture_data[];
};

uniform vec2 u_fixture_uv[10];
uniform float u_time;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    float nx = float(pixel.x) / float(imageSize(u_field).x);
    float ny = float(pixel.y) / float(imageSize(u_field).y);

    float field_intensity = sin((nx * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;
    imageStore(u_field, pixel, vec4(field_intensity, field_intensity, field_intensity, 1.0));

    if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
        for (int i = 0; i < 10; i++) {
            float u = u_fixture_uv[i].x;
            float v = u_fixture_uv[i].y;

            float dimmer = sin(u_time + u * 3.14159) * 0.5 + 0.5;
            float pan   = 0.5;
            float tilt  = 0.5;
            float hsl_h = 0.0;
            float hsl_s = 1.0;
            float hsl_l = dimmer;

            int base = i * 8;
            fixture_data[base + 0] = dimmer;
            fixture_data[base + 2] = pan;
            fixture_data[base + 3] = tilt;
            fixture_data[base + 4] = hsl_h;
            fixture_data[base + 5] = hsl_s;
            fixture_data[base + 6] = hsl_l;
        }
    }
}
)glsl";

// Point rendering shaders
static const char* kPointVertexSrc = R"glsl(
#version 460
uniform vec2 u_screen_size;
in vec2 a_pos;
in vec3 a_color;
out vec3 v_color;
void main() {
    vec2 clip = (a_pos / u_screen_size) * 2.0 - 1.0;
    gl_Position = vec4(clip, 0.0, 1.0);
    v_color = a_color;
}
)glsl";

static const char* kPointFragmentSrc = R"glsl(
#version 460
in vec3 v_color;
out vec4 out_color;
void main() {
    out_color = vec4(v_color, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        fprintf(stderr, "Shader compile error (%u): %s\n", type, info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// ---------------------------------------------------------------------------
int main() {
    // SDL3 + OpenGL
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init failed\n"); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Experiment 04 — Visual Overlay",
                                           800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) { fprintf(stderr, "Window failed\n"); return 1; }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) { fprintf(stderr, "GL context failed\n"); return 1; }
    SDL_GL_SetSwapInterval(1);

    if (gl3wInit() != 0) { fprintf(stderr, "gl3wInit failed\n"); return 1; }

    // Compute shader
    GLuint compShader = compileShader(GL_COMPUTE_SHADER, kComputeShaderSrc);
    if (!compShader) return 1;
    GLuint compProgram = glCreateProgram();
    glAttachShader(compProgram, compShader);
    glLinkProgram(compProgram);
    { GLint ok; glGetProgramiv(compProgram, GL_LINK_STATUS, &ok); if (!ok) return 1; }
    glDeleteShader(compShader);

    // Field texture (RGBA32F)
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kTexWidth, kTexHeight, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;

    // SSBO
    GLuint fixtureSSBO;
    glGenBuffers(1, &fixtureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kNumFixtures * sizeof(GPUFixureOutput), nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, fixtureSSBO);

    float fixtureUVs[kNumFixtures * 2] = {
        0.32f, 0.15f,  0.28f, 0.30f,  0.24f, 0.45f,  0.20f, 0.60f,  0.16f, 0.75f,
        0.68f, 0.15f,  0.72f, 0.30f,  0.76f, 0.45f,  0.80f, 0.60f,  0.84f, 0.75f,
    };
    GLint fixtureUVLoc = glGetUniformLocation(compProgram, "u_fixture_uv[0]");

    // Point shaders
    GLuint pointVS = compileShader(GL_VERTEX_SHADER,   kPointVertexSrc);
    GLuint pointFS = compileShader(GL_FRAGMENT_SHADER, kPointFragmentSrc);
    GLuint pointProgram = glCreateProgram();
    glAttachShader(pointProgram, pointVS);
    glAttachShader(pointProgram, pointFS);
    glLinkProgram(pointProgram);
    glDeleteShader(pointVS); glDeleteShader(pointFS);

    GLuint pointVAO, pointVBO;
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferData(GL_ARRAY_BUFFER, kNumFixtures * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    printf("Experiment 04 — Visual Overlay ready.\n");

    // Main loop
    float time_val = 0.0f;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
        }

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        glUseProgram(compProgram);
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);
        glUniform2fv(fixtureUVLoc, kNumFixtures, fixtureUVs);
        glDispatchCompute((kTexWidth+15)/16, (kTexHeight+15)/16, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, kTexWidth, kTexHeight, 0, 0, winW, winH,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
        GPUFixureOutput* mapped = (GPUFixureOutput*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (mapped) {
            std::vector<float> pointData;
            pointData.reserve(kNumFixtures * 5);
            for (int i = 0; i < kNumFixtures; ++i) {
                float x = fixtureUVs[i*2] * winW;
                float y = fixtureUVs[i*2+1] * winH;
                // Pure red, brightness = dimmer
                float r = mapped[i].dimmer;
                float g = 0.0f;
                float b = 0.0f;
                pointData.push_back(x);
                pointData.push_back(y);
                pointData.push_back(r);
                pointData.push_back(g);
                pointData.push_back(b);
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

            glBindVertexArray(pointVAO);
            glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, pointData.size() * sizeof(float), pointData.data());
            glUseProgram(pointProgram);
            glUniform2f(glGetUniformLocation(pointProgram, "u_screen_size"), (float)winW, (float)winH);
            glPointSize(12.0f);
            glDisable(GL_DEPTH_TEST);
            glDrawArrays(GL_POINTS, 0, kNumFixtures);
            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(window);
        time_val += 0.016f;
    }

    // Cleanup
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteProgram(pointProgram);
    glDeleteBuffers(1, &fixtureSSBO);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(compProgram);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
