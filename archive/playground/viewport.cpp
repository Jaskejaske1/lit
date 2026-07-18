#include "viewport.h"
#include "probe_manager.h"
#include <GL/gl3w.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ------------------------------------------------------------
// Matrix helpers (unchanged)
// ------------------------------------------------------------
using Mat4 = float[16];

static void mat4_identity(Mat4& m) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_perspective(float fov_y, float aspect, float near, float far, Mat4& m) {
    mat4_identity(m);
    float f = 1.0f / tanf(fov_y * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
    m[15] = 0.0f;
}

static void mat4_lookat(float eyeX, float eyeY, float eyeZ,
                        float targetX, float targetY, float targetZ,
                        float upX, float upY, float upZ,
                        Mat4& m) {
    float fx = targetX - eyeX;
    float fy = targetY - eyeY;
    float fz = targetZ - eyeZ;
    float f_len = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= f_len; fy /= f_len; fz /= f_len;

    float ux = upX, uy = upY, uz = upZ;
    float sx = fy*uz - fz*uy;
    float sy = fz*ux - fx*uz;
    float sz = fx*uy - fy*ux;
    float s_len = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= s_len; sy /= s_len; sz /= s_len;

    ux = sy*fz - sz*fy;
    uy = sz*fx - sx*fz;
    uz = sx*fy - sy*fx;

    m[0] = sx; m[1] = ux; m[2] = -fx; m[3] = 0.0f;
    m[4] = sy; m[5] = uy; m[6] = -fy; m[7] = 0.0f;
    m[8] = sz; m[9] = uz; m[10]= -fz; m[11]= 0.0f;
    m[12]= -sx*eyeX - sy*eyeY - sz*eyeZ;
    m[13]= -ux*eyeX - uy*eyeY - uz*eyeZ;
    m[14]= fx*eyeX + fy*eyeY + fz*eyeZ;
    m[15]= 1.0f;
}

static void mat4_multiply(const Mat4& a, const Mat4& b, Mat4& out) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col*4 + row] = a[row] * b[col*4]
                             + a[4+row] * b[col*4+1]
                             + a[8+row] * b[col*4+2]
                             + a[12+row] * b[col*4+3];
        }
    }
}

// ------------------------------------------------------------
// Shaders
// ------------------------------------------------------------
static const char* vertex_shader_src = R"glsl(
#version 460
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
uniform mat4 u_mvp;
out vec3 v_color;
void main() {
    gl_Position = u_mvp * vec4(position, 1.0);
    gl_PointSize = 8.0;
    v_color = color;
}
)glsl";

static const char* fragment_shader_src = R"glsl(
#version 460
in vec3 v_color;
out vec4 out_color;
void main() {
    out_color = vec4(v_color, 1.0);
}
)glsl";

// ------------------------------------------------------------
bool Viewport3D::initialize() {
    compile_shaders();
    if (!shader_program_) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    return true;
}

void Viewport3D::shutdown() {
    if (fbo_depth_rb_) glDeleteRenderbuffers(1, &fbo_depth_rb_);
    if (fbo_texture_)   glDeleteTextures(1, &fbo_texture_);
    if (fbo_)           glDeleteFramebuffers(1, &fbo_);
    if (vbo_)           glDeleteBuffers(1, &vbo_);
    if (vao_)           glDeleteVertexArrays(1, &vao_);
    if (shader_program_) glDeleteProgram(shader_program_);
}

void Viewport3D::compile_shaders() {
    auto compile = [](GLenum type, const char* src) -> unsigned int {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info[512];
            glGetShaderInfoLog(shader, 512, nullptr, info);
            fprintf(stderr, "Shader compile error: %s\n", info);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    unsigned int vs = compile(GL_VERTEX_SHADER, vertex_shader_src);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragment_shader_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(prog, 512, nullptr, info);
        fprintf(stderr, "Shader link error: %s\n", info);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    shader_program_ = prog;
}

void Viewport3D::setup_framebuffer(int width, int height) {
    // If size hasn't changed, reuse the existing FBO
    if (fbo_width_ == width && fbo_height_ == height) return;

    // Delete old resources if they exist
    if (fbo_depth_rb_) glDeleteRenderbuffers(1, &fbo_depth_rb_);
    if (fbo_texture_)   glDeleteTextures(1, &fbo_texture_);
    if (fbo_)           glDeleteFramebuffers(1, &fbo_);

    fbo_width_  = width;
    fbo_height_ = height;

    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &fbo_texture_);
    glGenRenderbuffers(1, &fbo_depth_rb_);

    // Color texture
    glBindTexture(GL_TEXTURE_2D, fbo_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Depth renderbuffer (needed so points don't overlap incorrectly)
    glBindRenderbuffer(GL_RENDERBUFFER, fbo_depth_rb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    // Attach to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo_depth_rb_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
    }

    // Unbind FBO (back to default framebuffer = screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewport3D::render_to_texture(const std::vector<ProbeRenderData>& probes,
                                   int width, int height) {
    if (width <= 0 || height <= 0 || probes.empty()) return;

    setup_framebuffer(width, height);

    // Save current state
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    GLint prev_fbo;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
    GLboolean scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint prev_scissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, prev_scissor);

    // Render to our FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, height);

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    render_gl(probes, width, height);

    glDisable(GL_DEPTH_TEST);

    // Restore previous state
    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    glScissor(prev_scissor[0], prev_scissor[1], prev_scissor[2], prev_scissor[3]);
    if (scissor_enabled) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void Viewport3D::render_gl(const std::vector<ProbeRenderData>& probes,
                           int width, int height) {
    update_buffers(probes);
    set_matrices(width, height);

    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, (GLsizei)probes.size());
    glBindVertexArray(0);
}

void Viewport3D::draw_imgui(const std::vector<ProbeRenderData>& probes) {
    // Get the available size for the viewport in ImGui
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x <= 0 || size.y <= 0) return;

    // Update the offscreen texture (only if size changed or probes updated)
    render_to_texture(probes, (int)size.x, (int)size.y);

    // Display the texture. ImGui::Image takes an OpenGL texture ID.
    // (ImTextureID is just a cast, we use intptr_t for 64-bit safety.)
    ImGui::Image((ImTextureID)(intptr_t)fbo_texture_, size, ImVec2(0,1), ImVec2(1,0));
}

void Viewport3D::handle_mouse(ImVec2 mouse_pos, bool left_down, float wheel) {
    if (left_down) {
        if (!dragging_) {
            dragging_ = true;
            last_mouse_x_ = mouse_pos.x;
            last_mouse_y_ = mouse_pos.y;
        } else {
            float dx = mouse_pos.x - last_mouse_x_;
            float dy = mouse_pos.y - last_mouse_y_;
            theta_ += dx * 0.005f;
            phi_   += dy * 0.005f;
            phi_ = std::clamp(phi_, -1.5f, 1.5f);
            last_mouse_x_ = mouse_pos.x;
            last_mouse_y_ = mouse_pos.y;
        }
    } else {
        dragging_ = false;
    }
    distance_ -= wheel * 0.1f;
    distance_ = std::clamp(distance_, 0.3f, 8.0f);
}

void Viewport3D::update_buffers(const std::vector<ProbeRenderData>& probes) {
    std::vector<float> data;
    data.reserve(probes.size() * 6);
    for (const auto& p : probes) {
        data.push_back(p.position[0]);
        data.push_back(p.position[1]);
        data.push_back(p.position[2]);
        data.push_back(p.color[0]);
        data.push_back(p.color[1]);
        data.push_back(p.color[2]);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Viewport3D::set_matrices(int width, int height) {
    float aspect = (float)width / (float)std::max(height, 1);
    Mat4 proj, view, mvp;
    mat4_perspective(60.0f * (float)M_PI / 180.0f, aspect, 0.01f, 100.0f, proj);

    float eye_x = target_x_ + distance_ * sinf(theta_) * cosf(phi_);
    float eye_y = target_y_ + distance_ * sinf(phi_);
    float eye_z = target_z_ + distance_ * cosf(theta_) * cosf(phi_);
    mat4_lookat(eye_x, eye_y, eye_z, target_x_, target_y_, target_z_, 0.0f, 1.0f, 0.0f, view);

    mat4_multiply(proj, view, mvp);

    glUseProgram(shader_program_);
    int mvp_loc = glGetUniformLocation(shader_program_, "u_mvp");
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp);
}
