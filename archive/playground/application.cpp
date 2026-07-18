#include "application.h"
#include <cstdio>

using namespace substrate;

bool Application::init() {
    // --- Substrate ---
    register_builtin_node_types();

    // --- Seed default diagonal sweep (same patch as lit_view) ---
    DefaultSpatialPatchIds ids;
    if (!seed_default_spatial_patch(graph_, next_id_, next_connection_id_, &ids)) {
        fprintf(stderr, "Failed to seed default patch\n");
        return false;
    }
    // IDs are not used after seeding for now, but we'll need the driver ID for ProbeManager.
    NodeId driver_node_id = ids.spatial_fixture_driver_node_id;

    // --- SDL3 + OpenGL 4.6 Core ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window_ = SDL_CreateWindow("lit Playground — Substrate Lab",
                               1280, 800,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    if (gl3wInit() != 0) {
        fprintf(stderr, "gl3wInit failed\n");
        return false;
    }

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 460");

    // --- Probe Manager ---
    probe_manager_.initialize(graph_, driver_node_id);

    // --- Viewport ---
    if (!viewport_.initialize()) {
        fprintf(stderr, "Viewport3D initialization failed\n");
        return false;
    }

    last_tick_time_ = (double)SDL_GetTicks() / 1000.0;
    return true;
}

void Application::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) running_ = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running_ = false;
                break;
        }
    }
}

void Application::update(float dt) {
    // Tick the main graph (needed for the UI to show live values)
    GraphBuildError err;
    if (!graph_.tick(dt, &err)) {
        fprintf(stderr, "Main graph tick error: %s\n", graph_build_error_name(err.code).data());
        running_ = false;
        return;
    }

    // Update all probe graphs
    probe_manager_.update(dt);
}

void Application::draw_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Full‑window panel with 3 columns
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("lit Playground", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Columns(3, "mainColumns", true);
    ImGui::SetColumnWidth(0, std::max(220.0f, ImGui::GetWindowWidth() * 0.2f));
    ImGui::SetColumnWidth(1, std::max(400.0f, ImGui::GetWindowWidth() * 0.5f));
    ImGui::SetColumnWidth(2, std::max(220.0f, ImGui::GetWindowWidth() * 0.2f));

    // --- Left: Graph Panel (placeholder) ---
    ImGui::Text("Graph Editor");
    ImGui::Text("Nodes: %zu", graph_.nodes.size());
    ImGui::Text("Connections: %zu", graph_.connections.size());
    ImGui::Text("Elapsed: %.2f s", graph_.elapsed_seconds);

    ImGui::NextColumn();

    // --- Center: 3D Viewport ---
    ImGui::BeginChild("ViewportChild", ImVec2(0, 0), ImGuiChildFlags_Border);
    {
        ImVec2 vp_size = ImGui::GetContentRegionAvail();
        const float min_viewport_size = 100.0f;
        if (vp_size.x > min_viewport_size && vp_size.y > min_viewport_size) {
            viewport_.draw_imgui(probe_manager_.get_render_data());

            if (ImGui::IsItemHovered()) {
                ImGuiIO& io = ImGui::GetIO();
                viewport_.handle_mouse(io.MousePos,
                                       ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                       io.MouseWheel);
            }
        } else {
            ImGui::TextUnformatted("Viewport too small");
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    // --- Right: Inspector (placeholder) ---
    ImGui::Text("Inspector");
    ImGui::Text("Select a node to inspect.");

    ImGui::End(); // main window

    ImGui::Render();
    int display_w, display_h;
    SDL_GetWindowSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void Application::run() {
    while (running_) {
        process_events();
        double now = (double)SDL_GetTicks() / 1000.0;
        float  dt  = (float)(now - last_tick_time_);
        last_tick_time_ = now;
        if (dt > 0.0f) update(dt);
        draw_ui();
    }
}

void Application::shutdown() {
    viewport_.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (gl_context_) SDL_GL_DestroyContext(gl_context_);
    if (window_)     SDL_DestroyWindow(window_);
    SDL_Quit();
}
