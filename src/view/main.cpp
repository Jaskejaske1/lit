// lit_view — substrate dev console (Phase 1.a)
//
// Minimum host shell: window + OpenGL 4.6 + ImGui + a debug panel that
// inspects the substrate's data model. No graph eval yet — that lands
// with Connection + Graph in the next step.
//
// This is the seed of the eventual Builder binary. For now it's a visual
// substrate debugger: you can see registered types, spawn demo nodes,
// and inspect their sockets + state in ImGui.
//
// Toolchain: SDL3 + OpenGL 4.6 (CORE) + ImGui (SDL3 + OpenGL3 backends) + gl3w.
// Validated by Phase 0, reused here with the new substrate on top.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <substrate/substrate.h>

// --- GPU preference hints (linker-level; must live in the executable) ---
// Hybrid-laptop trick: force the discrete NVIDIA / AMD GPU. Without these,
// the app can silently run on integrated graphics and tank performance.
// See docs/engineering-patterns.txt.
#ifdef _WIN32
#include <windows.h>
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

using namespace substrate;

// ============================================================================
// Demo node types
// ============================================================================
// Duplicated from tests/test_substrate.cpp. Premature to share — when the
// substrate ships real Phase / Decay / etc., those live in the library.

void constant_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = self.state.at("value");
}

void register_constant_demo() {
    NodeType t;
    t.name         = "Constant";
    t.display_name = "Constant";
    t.category     = "Generator";
    t.outputs.push_back({"Value", ValueType::Scalar, SocketValue{Scalar{0.0f}}, std::nullopt});
    t.state_schema.push_back({"value", ValueType::Scalar, SocketValue{Scalar{0.5f}}});
    t.evaluate = &constant_evaluate;
    register_node_type(t);
}

void constant_vec3_evaluate(Node& self, float, float, bool) {
    self.outputs[0].current = self.state.at("value");
}

void register_constant_vec3_demo() {
    NodeType t;
    t.name         = "ConstantVec3";
    t.display_name = "Constant Vec3";
    t.category     = "Generator";
    t.outputs.push_back({"Value", ValueType::Vec3, SocketValue{Vec3{0.0f, 0.0f, 0.0f}}, std::nullopt});
    t.state_schema.push_back({"value", ValueType::Vec3, SocketValue{Vec3{1.0f, 0.5f, 0.25f}}});
    t.evaluate = &constant_vec3_evaluate;
    register_node_type(t);
}

// ============================================================================
// Display helpers
// ============================================================================

// Format any SocketValue via std::visit. Add a branch when the variant grows.
std::string format_value(const SocketValue& v) {
    char buf[128];
    std::visit([&](auto&& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Scalar>) {
            snprintf(buf, sizeof(buf), "%.4f", x);
        } else if constexpr (std::is_same_v<T, Vec2>) {
            snprintf(buf, sizeof(buf), "[%.3f, %.3f]", x[0], x[1]);
        } else if constexpr (std::is_same_v<T, Vec3>) {
            snprintf(buf, sizeof(buf), "[%.3f, %.3f, %.3f]", x[0], x[1], x[2]);
        } else if constexpr (std::is_same_v<T, Vec4>) {
            snprintf(buf, sizeof(buf), "[%.3f, %.3f, %.3f, %.3f]", x[0], x[1], x[2], x[3]);
        } else {
            snprintf(buf, sizeof(buf), "?");
        }
    }, v);
    return std::string(buf);
}

const char* value_type_name(ValueType t) {
    switch (t) {
        case ValueType::Scalar: return "Scalar";
        case ValueType::Vec2:   return "Vec2";
        case ValueType::Vec3:   return "Vec3";
        case ValueType::Vec4:   return "Vec4";
    }
    return "?";
}

// ============================================================================
// Application
// ============================================================================

struct App {
    SDL_Window*   window     = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool          running    = true;

    std::vector<Node> nodes;
    NodeId            next_id = 1;

    // Per-frame tick state. Placeholder until Connection + Graph lands —
    // for now we drive each node's evaluate() directly so the Output values
    // actually reflect State (currently identical for Constant types; will
    // get interesting once Phase / Decay / etc. exist).
    double last_tick_time = 0.0;
    double elapsed        = 0.0;

    void tick(float dt);

    bool init();
    void run();
    void shutdown();

    void draw_debug_panel();
    void draw_node(Node& n);
    void spawn_node(const char* type_name);
};

bool App::init() {
    // 1. Register demo node types into the substrate's global registry.
    register_constant_demo();
    register_constant_vec3_demo();

    // 2. SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // 3. OpenGL 4.6 CORE (validated Phase 0 config)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // 4. Window
    window = SDL_CreateWindow("lit_view - Substrate Dev Console",
                              1280, 800,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // 5. GL context + VSync
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    // 6. gl3w — loads OpenGL function pointers
    if (gl3wInit() != 0) {
        fprintf(stderr, "gl3wInit failed\n");
        return false;
    }

    fprintf(stderr, "=== lit_view ===\n");
    fprintf(stderr, "Vendor:   %s\n", glGetString(GL_VENDOR));
    fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "Version:  %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "===============\n");

    // 7. ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 460");

    // 8. Spawn a few demo nodes so the user sees something on first frame
    spawn_node("Constant");
    spawn_node("ConstantVec3");
    spawn_node("Constant");

    // 9. Seed the per-frame clock
    last_tick_time = (double)SDL_GetTicks() / 1000.0;

    return true;
}

void App::tick(float dt) {
    // Placeholder eval loop — runs every node's evaluate() in registration
    // order. Will be replaced by Graph::tick() once Connection + Graph land.
    // Bypass is honored here so a node can be muted from the host too.
    for (auto& n : nodes) {
        if (n.type && n.type->evaluate && !n.bypass) {
            n.type->evaluate(n, dt, (float)elapsed, /*init_pass=*/false);
        }
    }
}

void App::spawn_node(const char* type_name) {
    const NodeType* t = find_node_type(type_name);
    if (!t) {
        fprintf(stderr, "spawn_node: type '%s' not found\n", type_name);
        return;
    }
    std::string instance_name = std::string(type_name) + " #" + std::to_string(next_id);
    nodes.push_back(make_node(*t, next_id++, instance_name));
}

void App::draw_node(Node& n) {
    if (!ImGui::TreeNode(&n, "Node #%llu  '%s'  [%s]",
                         (unsigned long long)n.id,
                         n.name.c_str(),
                         n.type->name.c_str())) {
        return;
    }

    ImGui::Text("Position: (%.1f, %.1f)", n.position[0], n.position[1]);
    ImGui::Text("Bypass:   %s", n.bypass ? "true" : "false");

    if (!n.inputs.empty()) {
        if (ImGui::TreeNode("Inputs")) {
            for (size_t i = 0; i < n.inputs.size(); ++i) {
                const Socket& s = n.inputs[i];
                ImGui::BulletText("[%zu] %-12s %-6s = %s",
                                  i, s.name.c_str(), value_type_name(s.type),
                                  format_value(s.current).c_str());
            }
            ImGui::TreePop();
        }
    }

    if (!n.outputs.empty()) {
        if (ImGui::TreeNode("Outputs")) {
            for (size_t i = 0; i < n.outputs.size(); ++i) {
                const Socket& s = n.outputs[i];
                ImGui::BulletText("[%zu] %-12s %-6s = %s",
                                  i, s.name.c_str(), value_type_name(s.type),
                                  format_value(s.current).c_str());
            }
            ImGui::TreePop();
        }
    }

    if (!n.state.empty()) {
        if (ImGui::TreeNode("State")) {
            for (const auto& [key, val] : n.state) {
                // State values don't carry their type on the Node; look it up
                // in the type's state_schema.
                ValueType vt = ValueType::Scalar;
                for (const auto& spec : n.type->state_schema) {
                    if (spec.name == key) {
                        vt = spec.type;
                        break;
                    }
                }
                ImGui::BulletText("%-12s %-6s = %s",
                                  key.c_str(), value_type_name(vt),
                                  format_value(val).c_str());
            }
            ImGui::TreePop();
        }
    }

    ImGui::TreePop();
}

void App::draw_debug_panel() {
    ImGui::Begin("lit_view - Substrate Dev Console");

    // Registered types
    const auto& all = all_node_types();
    if (ImGui::CollapsingHeader("Registered Node Types", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Count: %zu", all.size());
        for (const auto& [name, type] : all) {
            ImGui::BulletText("%-18s  category: %-12s  in/out: %zu/%zu",
                              name.c_str(), type.category.c_str(),
                              type.inputs.size(), type.outputs.size());
        }
    }

    // Spawn controls
    ImGui::Separator();
    ImGui::Text("Spawn:");
    if (ImGui::Button("Constant"))      spawn_node("Constant");
    ImGui::SameLine();
    if (ImGui::Button("Constant Vec3")) spawn_node("ConstantVec3");

    // Demo nodes
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Count: %zu", nodes.size());
        for (auto& n : nodes) {
            draw_node(n);
        }
    }

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Elapsed: %.3f s", elapsed);
    ImGui::Text("Press ESC to quit.");
    ImGui::End();
}

void App::run() {
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }

        // Compute dt, accumulate elapsed, tick the substrate.
        double now = (double)SDL_GetTicks() / 1000.0;
        float  dt  = (float)(now - last_tick_time);
        last_tick_time = now;
        elapsed += dt;
        if (dt > 0.0f) {
            tick(dt);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        draw_debug_panel();

        ImGui::Render();
        int display_w, display_h;
        SDL_GetWindowSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }
}

void App::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (gl_context) SDL_GL_DestroyContext(gl_context);
    if (window)     SDL_DestroyWindow(window);

    SDL_Quit();
}

int main(int /*argc*/, char* /*argv*/[]) {
    App app;
    if (!app.init()) {
        fprintf(stderr, "lit_view init failed\n");
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
