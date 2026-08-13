// experiment_08 — 3D Fixture Preview with a Moving Radial Energy Field
//
// A meaningful 3D field preview:
//   - 12 fixtures in true 3D positions (three columns, four heights)
//   - a moving radial energy source
//   - fixtures sample the field at their world positions
//   - the field is visible as a sparse 3D point cloud
//   - floor reference grid gives spatial orientation
//
// Controls:
//   drag left mouse = orbit
//   scroll wheel    = zoom
//   Esc             = quit

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
constexpr int kNumFixtures = 12;

// Field cloud resolution
constexpr int kFieldX = 12;
constexpr int kFieldY = 8;
constexpr int kFieldZ = 6;
constexpr int kFieldCount = kFieldX * kFieldY * kFieldZ;

// ---------------------------------------------------------------------------
// GL error helper
// ---------------------------------------------------------------------------
static void checkGlError(const char *where)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        fprintf(stderr, "GL error after %s: 0x%04X\n", where, err);
    }
}

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

static const char *kComputeShaderSrc = R"glsl(
#version 460
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer u_positions {
    float positions[];      // flat: 3 floats per sample
};

layout(std430, binding = 1) buffer u_values {
    float values[];
};

uniform float u_time;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= values.length()) return;

    vec3 p = vec3(
        positions[i * 3 + 0],
        positions[i * 3 + 1],
        positions[i * 3 + 2]
    );

    // Moving radial energy source.
    vec3 source = vec3(
        0.5 + 0.22 * sin(u_time * 0.8),
        0.5 + 0.30 * sin(u_time * 0.5),
        0.05 + 0.15 * sin(u_time * 0.6)
    );

    float radius = 0.28;
    float d2 = dot(p - source, p - source);
    float v = exp(-d2 / (radius * radius));

    values[i] = clamp(v, 0.0, 1.0);
}
)glsl";

static const char *kPointVertexSrc = R"glsl(
#version 460
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
uniform mat4 u_proj;
uniform mat4 u_view;
out vec3 v_color;
void main() {
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
    v_color = a_color;
}
)glsl";

static const char *kPointFragmentSrc = R"glsl(
#version 460
in vec3 v_color;
out vec4 out_color;
void main() {
    out_color = vec4(v_color, 1.0);
}
)glsl";

static const char *kLineVertexSrc = R"glsl(
#version 460
layout(location = 0) in vec3 a_pos;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)glsl";

static const char *kLineFragmentSrc = R"glsl(
#version 460
out vec4 out_color;
uniform vec3 u_color;
void main() {
    out_color = vec4(u_color, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Shader helpers
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

static GLuint linkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        fprintf(stderr, "Program link error: %s\n", info);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ---------------------------------------------------------------------------
// Matrix helpers
// ---------------------------------------------------------------------------
static void mat4Perspective(float *m, float fovY, float aspect,
                            float zNear, float zFar)
{
    float f = 1.0f / tanf(fovY * 0.5f);

    m[0] = f / aspect;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;
    m[4] = 0.0f;
    m[5] = f;
    m[6] = 0.0f;
    m[7] = 0.0f;
    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0f;
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    m[15] = 0.0f;
}

static void mat4LookAt(float *m,
                       float eyeX, float eyeY, float eyeZ,
                       float centerX, float centerY, float centerZ,
                       float upX, float upY, float upZ)
{
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    float fLen = sqrtf(fx * fx + fy * fy + fz * fz);
    fx /= fLen;
    fy /= fLen;
    fz /= fLen;

    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float sLen = sqrtf(sx * sx + sy * sy + sz * sz);
    sx /= sLen;
    sy /= sLen;
    sz /= sLen;

    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    m[0] = sx;
    m[4] = ux;
    m[8] = -fx;
    m[12] = 0.0f;
    m[1] = sy;
    m[5] = uy;
    m[9] = -fy;
    m[13] = 0.0f;
    m[2] = sz;
    m[6] = uz;
    m[10] = -fz;
    m[14] = 0.0f;
    m[3] = 0.0f;
    m[7] = 0.0f;
    m[11] = 0.0f;
    m[15] = 1.0f;

    m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
}

static void mat4Multiply(float *out, const float *a, const float *b)
{
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

// ---------------------------------------------------------------------------
// Heat colour: dark blue → orange → yellow → red
// ---------------------------------------------------------------------------
static void heatColor(float t, float &r, float &g, float &b)
{
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    // Dark blue → orange → yellow → red.
    // High field values become red, mid values become yellow/orange,
    // low values fade into the dark blue background.
    if (t < 0.15f)
    {
        float x = t / 0.15f;
        r = 0.0f;
        g = 0.0f;
        b = 0.08f + 0.25f * x;
    }
    else if (t < 0.45f)
    {
        float x = (t - 0.15f) / 0.30f;
        r = 0.95f * x;
        g = 0.20f * x;
        b = 0.33f - 0.28f * x;
    }
    else if (t < 0.75f)
    {
        float x = (t - 0.45f) / 0.30f;
        r = 0.95f;
        g = 0.20f + 0.65f * x;
        b = 0.05f - 0.05f * x;
    }
    else
    {
        float x = (t - 0.75f) / 0.25f;
        r = 1.0f;
        g = 0.85f - 0.85f * x;
        b = 0.0f;
    }
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
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Experiment 08 - 3D Fixture Preview",
                                          960, 720,
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

    // 2. Compile programs
    GLuint compVS = compileShader(GL_COMPUTE_SHADER, kComputeShaderSrc);
    if (!compVS)
        return 1;

    GLuint compProgram = glCreateProgram();
    glAttachShader(compProgram, compVS);
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
    glDeleteShader(compVS);

    GLuint pointVS = compileShader(GL_VERTEX_SHADER, kPointVertexSrc);
    GLuint pointFS = compileShader(GL_FRAGMENT_SHADER, kPointFragmentSrc);
    if (!pointVS || !pointFS)
        return 1;
    GLuint pointProgram = linkProgram(pointVS, pointFS);
    if (!pointProgram)
        return 1;

    GLuint lineVS = compileShader(GL_VERTEX_SHADER, kLineVertexSrc);
    GLuint lineFS = compileShader(GL_FRAGMENT_SHADER, kLineFragmentSrc);
    if (!lineVS || !lineFS)
        return 1;
    GLuint lineProgram = linkProgram(lineVS, lineFS);
    if (!lineProgram)
        return 1;

    // 3. Build sample positions: field cloud first, fixtures second.
    std::vector<float> fieldPositions;
    fieldPositions.reserve(kFieldCount * 3);

    for (int zi = 0; zi < kFieldZ; ++zi)
    {
        float z = -0.15f + 0.25f * ((float)zi / (float)(kFieldZ - 1));
        for (int yi = 0; yi < kFieldY; ++yi)
        {
            float y = 0.05f + 0.95f * ((float)yi / (float)(kFieldY - 1));
            for (int xi = 0; xi < kFieldX; ++xi)
            {
                float x = 0.05f + 0.90f * ((float)xi / (float)(kFieldX - 1));
                fieldPositions.push_back(x);
                fieldPositions.push_back(y);
                fieldPositions.push_back(z);
            }
        }
    }

    // Fixture positions: three columns, four heights each, with varying Z.
    const float colX[3] = {0.20f, 0.50f, 0.80f};
    const float rowY[4] = {0.10f, 0.40f, 0.70f, 1.00f};
    const float rowZ[4] = {-0.15f, -0.05f, 0.05f, 0.15f};

    std::vector<float> fixturePositions;
    fixturePositions.reserve(kNumFixtures * 3);

    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            fixturePositions.push_back(colX[col]);
            fixturePositions.push_back(rowY[row]);
            fixturePositions.push_back(rowZ[row] + 0.03f * col);
        }
    }

    // Combined positions for the compute shader.
    std::vector<float> allPositions = fieldPositions;
    allPositions.insert(allPositions.end(),
                        fixturePositions.begin(),
                        fixturePositions.end());

    // 4. SSBOs
    GLuint posSSBO = 0;
    glGenBuffers(1, &posSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, posSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 allPositions.size() * sizeof(float),
                 allPositions.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posSSBO);

    GLuint valSSBO = 0;
    glGenBuffers(1, &valSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, valSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (kFieldCount + kNumFixtures) * sizeof(float),
                 nullptr, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, valSSBO);

    // 5. VAOs/VBOs for field cloud and fixtures.
    GLuint fieldVAO = 0, fieldPosVBO = 0, fieldColorVBO = 0;
    glGenVertexArrays(1, &fieldVAO);
    glGenBuffers(1, &fieldPosVBO);
    glGenBuffers(1, &fieldColorVBO);

    glBindVertexArray(fieldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fieldPosVBO);
    glBufferData(GL_ARRAY_BUFFER, fieldPositions.size() * sizeof(float),
                 fieldPositions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, fieldColorVBO);
    glBufferData(GL_ARRAY_BUFFER, kFieldCount * 3 * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    GLuint fixtureVAO = 0, fixturePosVBO = 0, fixtureColorVBO = 0;
    glGenVertexArrays(1, &fixtureVAO);
    glGenBuffers(1, &fixturePosVBO);
    glGenBuffers(1, &fixtureColorVBO);

    glBindVertexArray(fixtureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fixturePosVBO);
    glBufferData(GL_ARRAY_BUFFER, fixturePositions.size() * sizeof(float),
                 fixturePositions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, fixtureColorVBO);
    glBufferData(GL_ARRAY_BUFFER, kNumFixtures * 3 * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // 6. Floor reference grid.
    std::vector<float> lineVertices;
    auto addLine = [&](float ax, float ay, float az,
                       float bx, float by, float bz)
    {
        lineVertices.push_back(ax);
        lineVertices.push_back(ay);
        lineVertices.push_back(az);
        lineVertices.push_back(bx);
        lineVertices.push_back(by);
        lineVertices.push_back(bz);
    };

    const float x0 = 0.05f, x1 = 0.95f;
    const float z0 = -0.20f, z1 = 0.25f;
    const float floorY = 0.0f;

    for (int i = 0; i <= 6; ++i)
    {
        float x = x0 + (x1 - x0) * i / 6.0f;
        addLine(x, floorY, z0, x, floorY, z1);
    }
    for (int i = 0; i <= 6; ++i)
    {
        float z = z0 + (z1 - z0) * i / 6.0f;
        addLine(x0, floorY, z, x1, floorY, z);
    }

    GLuint lineVAO = 0, lineVBO = 0;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float),
                 lineVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    printf("Experiment 08 - 3D Fixture Preview ready.\n");
    printf("Drag left mouse to orbit. Scroll to zoom. Esc to quit.\n");

    // 7. Camera state.
    float yaw = 0.8f;
    float pitch = 0.55f;
    float distance = 2.4f;
    const float targetX = 0.5f;
    const float targetY = 0.5f;
    const float targetZ = 0.0f;

    bool leftDown = false;
    int lastMouseX = 0, lastMouseY = 0;

    // 8. Timing using performance counter.
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    // 9. Main loop.
    float time_val = 0.0f;
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

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                leftDown = true;
                lastMouseX = (int)event.button.x;
                lastMouseY = (int)event.button.y;
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                leftDown = false;
            }

            if (event.type == SDL_EVENT_MOUSE_MOTION && leftDown)
            {
                int mx = (int)event.motion.x;
                int my = (int)event.motion.y;
                yaw += (float)(mx - lastMouseX) * 0.010f;
                pitch -= (float)(my - lastMouseY) * 0.010f;
                if (pitch > 1.45f)
                    pitch = 1.45f;
                if (pitch < -1.45f)
                    pitch = -1.45f;
                lastMouseX = mx;
                lastMouseY = my;
            }

            if (event.type == SDL_EVENT_MOUSE_WHEEL)
            {
                distance -= event.wheel.y * 0.1f;
                if (distance < 0.7f)
                    distance = 0.7f;
                if (distance > 6.0f)
                    distance = 6.0f;
            }
        }

        Uint64 counter = SDL_GetPerformanceCounter();
        float dt = (float)(counter - lastCounter) / (float)freq;
        lastCounter = counter;
        if (dt <= 0.0f)
            dt = 0.016f;
        time_val += dt;

        // --- Compute pass ---
        glUseProgram(compProgram);
        glUniform1f(glGetUniformLocation(compProgram, "u_time"), time_val);
        glDispatchCompute((kFieldCount + kNumFixtures + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // --- Read back values ---
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, valSSBO);
        float *mapped = (float *)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (!mapped)
        {
            fprintf(stderr, "glMapBuffer failed\n");
            continue;
        }

        std::vector<float> values(kFieldCount + kNumFixtures);
        for (int i = 0; i < (int)values.size(); ++i)
        {
            values[i] = mapped[i];
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        // Print first 4 fixture samples.
        printf("\r");
        for (int i = 0; i < 4; ++i)
        {
            printf("Fixt %d: %.3f  ", i, values[kFieldCount + i]);
        }
        fflush(stdout);

        // Build field cloud colours.
        std::vector<float> fieldColors;
        fieldColors.reserve(kFieldCount * 3);
        for (int i = 0; i < kFieldCount; ++i)
        {
            float r, g, b;
            heatColor(values[i], r, g, b);
            fieldColors.push_back(r);
            fieldColors.push_back(g);
            fieldColors.push_back(b);
        }

        // Build fixture colours.
        std::vector<float> fixtureColors;
        fixtureColors.reserve(kNumFixtures * 3);
        for (int i = 0; i < kNumFixtures; ++i)
        {
            float r, g, b;
            heatColor(values[kFieldCount + i], r, g, b);
            fixtureColors.push_back(r);
            fixtureColors.push_back(g);
            fixtureColors.push_back(b);
        }

        // Upload colours.
        glBindBuffer(GL_ARRAY_BUFFER, fieldColorVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        fieldColors.size() * sizeof(float), fieldColors.data());

        glBindBuffer(GL_ARRAY_BUFFER, fixtureColorVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        fixtureColors.size() * sizeof(float), fixtureColors.data());

        // --- Render ---
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        if (winW == 0 || winH == 0)
        {
            SDL_GL_SwapWindow(window);
            continue;
        }

        glViewport(0, 0, winW, winH);
        glClearColor(0.01f, 0.02f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        float proj[16];
        mat4Perspective(proj, 60.0f * 3.14159f / 180.0f,
                        (float)winW / (float)winH, 0.05f, 20.0f);

        float eyeX = targetX + distance * cosf(pitch) * sinf(yaw);
        float eyeY = targetY + distance * sinf(pitch);
        float eyeZ = targetZ + distance * cosf(pitch) * cosf(yaw);

        float view[16];
        mat4LookAt(view, eyeX, eyeY, eyeZ,
                   targetX, targetY, targetZ,
                   0.0f, 1.0f, 0.0f);

        float mvp[16];
        mat4Multiply(mvp, proj, view);

        // Draw floor grid.
        glUseProgram(lineProgram);
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "u_mvp"), 1, GL_FALSE, mvp);
        glUniform3f(glGetUniformLocation(lineProgram, "u_color"), 0.22f, 0.22f, 0.30f);
        glBindVertexArray(lineVAO);
        glDrawArrays(GL_LINES, 0, (GLsizei)(lineVertices.size() / 3));
        glBindVertexArray(0);

        // Draw field cloud.
        glUseProgram(pointProgram);
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "u_proj"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(pointProgram, "u_view"), 1, GL_FALSE, view);
        glPointSize(6.0f);
        glBindVertexArray(fieldVAO);
        glDrawArrays(GL_POINTS, 0, kFieldCount);
        glBindVertexArray(0);

        // Draw fixtures.
        glPointSize(18.0f);
        glBindVertexArray(fixtureVAO);
        glDrawArrays(GL_POINTS, 0, kNumFixtures);
        glBindVertexArray(0);

        glDisable(GL_DEPTH_TEST);
        SDL_GL_SwapWindow(window);
    }

    // Cleanup.
    glDeleteVertexArrays(1, &fixtureVAO);
    glDeleteBuffers(1, &fixturePosVBO);
    glDeleteBuffers(1, &fixtureColorVBO);

    glDeleteVertexArrays(1, &fieldVAO);
    glDeleteBuffers(1, &fieldPosVBO);
    glDeleteBuffers(1, &fieldColorVBO);

    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);

    glDeleteProgram(pointProgram);
    glDeleteProgram(lineProgram);
    glDeleteProgram(compProgram);

    glDeleteBuffers(1, &valSSBO);
    glDeleteBuffers(1, &posSSBO);

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}