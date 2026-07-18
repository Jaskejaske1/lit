// experiment_01 — Compute shader sine field (blit version)
//
// Opens an SDL3 window, runs a GLSL compute shader that writes a moving
// sine wave into a 256x256 texture, then copies that texture to the screen
// using glBlitFramebuffer. No vertex/fragment shaders required.
//
// If the window stays black, the terminal will print diagnostic info.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/gl3w.h>

// ---------------------------------------------------------------------------
// Compute shader (writes into texture)
// ---------------------------------------------------------------------------
static const char* kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, r32f) uniform writeonly image2D u_output;
uniform float u_time;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    float nx = float(pixel.x) / float(imageSize(u_output).x);
    float ny = float(pixel.y) / float(imageSize(u_output).y);
    float value = sin((nx * 10.0 + u_time) * 3.14159) * 0.5 + 0.5;
    imageStore(u_output, pixel, vec4(value, 0.0, 0.0, 0.0));
}
)glsl";

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

    SDL_Window* window = SDL_CreateWindow("Experiment 01 — Compute Sine Field",
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

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

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
    printf("Compute shader compiled and linked.\n");

    // 3. Create the texture (256x256, R32F)
    const int kTexWidth = 256, kTexHeight = 256;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kTexWidth, kTexHeight, 0,
                 GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 4. Create an offscreen framebuffer to render the texture into
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
        return 1;
    }
    printf("FBO created, texture attached.\n");

    checkGlError("setup");

    // 5. Main loop
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
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        GLint timeLoc = glGetUniformLocation(compProgram, "u_time");
        glUniform1f(timeLoc, time_val);
        glDispatchCompute((kTexWidth+15)/16, (kTexHeight+15)/16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        checkGlError("compute dispatch");

        // --- Blit the texture to the screen ---
        // Bind our texture to the read framebuffer (already done, but ensure it)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        // The default framebuffer (0) is the screen
        glBlitFramebuffer(0, 0, kTexWidth, kTexHeight,
                          0, 0, 800, 600,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        checkGlError("blit");

        SDL_GL_SwapWindow(window);

        // Advance time
        time_val += 0.016f;
    }

    // Cleanup
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(compProgram);
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
