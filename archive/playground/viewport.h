#pragma once

#include <vector>
#include <substrate/substrate.h>
#include <imgui.h>

struct ProbeRenderData;

class Viewport3D {
public:
    Viewport3D() = default;
    ~Viewport3D() { shutdown(); }

    bool initialize();
    void shutdown();

    // Render probes to the offscreen FBO texture.
    // Must be called when the viewport size changes or probes update.
    void render_to_texture(const std::vector<ProbeRenderData>& probes,
                           int width, int height);

    // Call inside the ImGui panel to show the viewport texture.
    void draw_imgui(const std::vector<ProbeRenderData>& probes);

    // Camera controls (call while the viewport item is hovered).
    void handle_mouse(ImVec2 mouse_pos, bool left_down, float wheel);

private:
    // Shaders and vertex data
    unsigned int shader_program_ = 0;
    unsigned int vao_ = 0, vbo_ = 0;

    // Offscreen framebuffer
    unsigned int fbo_          = 0;
    unsigned int fbo_texture_  = 0;
    unsigned int fbo_depth_rb_ = 0;
    int          fbo_width_    = 0;
    int          fbo_height_   = 0;

    // Camera state
    float theta_    = 0.8f;
    float phi_      = 0.6f;
    float distance_ = 1.8f;
    float target_x_ = 0.5f, target_y_ = 0.5f, target_z_ = 0.0f;
    bool  dragging_     = false;
    float last_mouse_x_ = 0, last_mouse_y_ = 0;

    void compile_shaders();
    void setup_framebuffer(int width, int height);
    void render_gl(const std::vector<ProbeRenderData>& probes,
                   int width, int height);
    void update_buffers(const std::vector<ProbeRenderData>& probes);
    void set_matrices(int width, int height);
};
