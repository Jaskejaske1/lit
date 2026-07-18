// experiment_06 — Diagonal Red Sweep with stateful decay
//
// Same effect as before but now with proper per‑pixel and per‑fixture
// exponential decay. Two extra SSBOs hold the previous decay values.
// The CPU sends dt each frame so the shader can compute exp(-dt/tau).
//
// Proves: stateful GPU shader nodes are straightforward—just a buffer
// per state variable. This directly informs the substrate‑to‑shader
// compiler design.

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
// Compute shader with stateful decay
// ---------------------------------------------------------------------------
static const char* kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba32f) uniform writeonly image2D u_field;

layout(std430, binding = 1) buffer u_fixture_output {
    float fixture_data[];
};

// Per‑pixel decay state (size = width * height)
layout(std430, binding = 2) buffer u_field_decay {
    float field_decay[];
};

// Per‑fixture decay state (size = kNumFixtures)
layout(std430, binding = 3) buffer u_fixture_decay {
    float fixture_decay[];
};

uniform vec2 u_fixture_uv[10];
uniform float u_time;
uniform float u_dt;          // delta time in seconds

// ---- Effect parameters ----
const float BPM           = 100.0;
const float beatsPerSweep = 3.0;
const float sweepPeriod   = 60.0 / BPM * beatsPerSweep;

const vec3  barAxis       = vec3(0.47, 0.88, 0.18);
const float barOffset     = 0.0;

const float bandWidth     = 0.10;
const float bandFeather   = 0.10;

const float decayTau      = 0.28;

const float baseTilt = 0.30;
const float peakTilt = 0.82;

// -------------------------------------------------------------------
float barCoordinate(vec3 pos) {
    float mirroredX = abs(pos.x - 0.5) * 2.0;
    vec3  p = vec3(mirroredX, pos.y, pos.z);
    return clamp(dot(p, barAxis) + barOffset, 0.0, 1.0);
}

float bandWindow(float position, float center) {
    float dist = abs(position - center);
    dist = min(dist, 1.0 - dist);
    float halfW = bandWidth * 0.5;
    if (dist <= halfW) return 1.0;
    if (dist >= halfW + bandFeather) return 0.0;
    return 1.0 - (dist - halfW) / bandFeather;
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    int   pixelIndex = pixel.y * int(imageSize(u_field).x) + pixel.x;
    float nx = float(pixel.x) / float(imageSize(u_field).x);
    float ny = float(pixel.y) / float(imageSize(u_field).y);

    // ---- Sweep phase ----
    float sweepPhase = fract(u_time / sweepPeriod);

    // ---- Per‑pixel decay (background field) ----
    float pixelBarCoord = barCoordinate(vec3(nx, ny, 0.0));
    float inputSignal   = bandWindow(pixelBarCoord, sweepPhase);
    float prevDecay     = field_decay[pixelIndex];
    float newDecay;
    if (u_dt <= 0.0) {
        newDecay = inputSignal;
    } else {
        newDecay = max(inputSignal, prevDecay * exp(-u_dt / decayTau));
    }
    field_decay[pixelIndex] = newDecay;

    float fieldIntensity = newDecay;
    imageStore(u_field, pixel, vec4(fieldIntensity, fieldIntensity, fieldIntensity, 1.0));

    // ---- Per‑fixture output (only one thread per group) ----
    if (gl_LocalInvocationID.x == 0 && gl_LocalInvocationID.y == 0) {
        for (int i = 0; i < 10; i++) {
            float u = u_fixture_uv[i].x;
            float v = u_fixture_uv[i].y;
            vec3  fixturePos = vec3(u, v, 0.0);

            float barCoord   = barCoordinate(fixturePos);
            float inputSig   = bandWindow(barCoord, sweepPhase);
            float prev       = fixture_decay[i];
            float newDecayFixt;
            if (u_dt <= 0.0) {
                newDecayFixt = inputSig;
            } else {
                newDecayFixt = max(inputSig, prev * exp(-u_dt / decayTau));
            }
            fixture_decay[i] = newDecayFixt;

            float sweepIntensity = newDecayFixt;

            // Dimmer always full
            float dimmer = 1.0;
            // Tilt mix
            float tilt = mix(baseTilt, peakTilt, sweepIntensity);
            // Color: white -> red
            float hsl_h = 0.0;
            float hsl_s = 1.0;
            float hsl_l = mix(1.0, 0.5, sweepIntensity);

            int base = i * 8;
            fixture_data[base + 0] = dimmer;
            fixture_data[base + 2] = 0.5;   // pan
            fixture_data[base + 3] = tilt;
            fixture_data[base + 4] = hsl_h;
            fixture_data[base + 5] = hsl_s;
            fixture_data[base + 6] = hsl_l;
        }
    }
}
)glsl";

// Point rendering shaders (unchanged)
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

static float hue2rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f/2.0f) return q;
    if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
    return p;
}

static void hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    if (s == 0.0f) { r = g = b = l; return; }
    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    r = hue2rgb(p, q, h + 1.0f/3.0f);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0f/3.0f);
}

// ---------------------------------------------------------------------------
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init failed\n"); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Experiment 06 — Diagonal Red Sweep",
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

    // Fixture output SSBO
    GLuint fixtureSSBO;
    glGenBuffers(1, &fixtureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kNumFixtures * sizeof(GPUFixureOutput), nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, fixtureSSBO);

    // Per‑pixel decay state SSBO (binding 2)
    GLuint fieldDecaySSBO;
    glGenBuffers(1, &fieldDecaySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fieldDecaySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kTexWidth * kTexHeight * sizeof(float), nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, fieldDecaySSBO);

    // Per‑fixture decay state SSBO (binding 3)
    GLuint fixtureDecaySSBO;
    glGenBuffers(1, &fixtureDecaySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixtureDecaySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 kNumFixtures * sizeof(float), nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, fixtureDecaySSBO);

    // Fixture positions
    float fixtureUVs[kNumFixtures * 2] = {
        0.32f, 0.15f,  0.28f, 0.30f,  0.24f, 0.45f,  0.20f, 0.60f,  0.16f, 0.75f,
        0.68f, 0.15f,  0.72f, 0.30f,  0.76f, 0.45f,  0.80f, 0.60f,  0.84f, 0.75f,
    };
    GLint fixtureUVLoc = glGetUniformLocation(compProgram, "u_fixture_uv[0]");

    // Point rendering
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

    printf("Experiment 06 — Diagonal Red Sweep (stateful) ready.\n");

    float time_val = 0.0f;
    double lastTick = (double)SDL_GetTicks() / 1000.0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
        }

        double now = (double)SDL_GetTicks() / 1000.0;
        float dt = (float)(now - lastTick);
        lastTick = now;
        if (dt <= 0.0f) dt = 0.016f;   // safe fallback

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        glUseProgram(compProgram);
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);
        glUniform1f(glGetUniformLocation(compProgram, "u_dt"), dt);
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
                float r, g, b;
                hslToRgb(mapped[i].hsl_h, mapped[i].hsl_s, mapped[i].hsl_l, r, g, b);
                r *= mapped[i].dimmer;
                g *= mapped[i].dimmer;
                b *= mapped[i].dimmer;
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
        time_val += dt;
    }

    // Cleanup
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteProgram(pointProgram);
    glDeleteBuffers(1, &fixtureDecaySSBO);
    glDeleteBuffers(1, &fieldDecaySSBO);
    glDeleteBuffers(1, &fixtureSSBO);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(compProgram);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
