#pragma once

#include <SDL3/SDL.h>
#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <substrate/substrate.h>
#include "probe_manager.h"
#include "viewport.h"

class Application {
public:
    bool init();
    void run();
    void shutdown();

private:
    SDL_Window*   window_     = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool          running_    = true;

    substrate::Graph        graph_;
    substrate::NodeId       next_id_            = 1;
    substrate::ConnectionId next_connection_id_ = 1;
    double                  last_tick_time_     = 0.0;

    ProbeManager probe_manager_;
    Viewport3D   viewport_;

    void process_events();
    void update(float dt);
    void draw_ui();
};
