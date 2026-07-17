// lit_view — substrate dev console (Phase 1.a)
//
// Minimum host shell: window + OpenGL 4.6 + ImGui + a debug panel that
// inspects the substrate's data model while ticking a real Graph.
//
// This is the seed of the eventual Builder binary. For now it's a visual
// substrate debugger: you can see registered types, spawn demo nodes,
// and inspect their sockets + state in ImGui.
//
// Toolchain: SDL3 + OpenGL 4.6 (CORE) + ImGui (SDL3 + OpenGL3 backends) + gl3w.
// Validated by Phase 0, reused here with the new substrate on top.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>
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

bool edit_value_widget(const char* label,
                       ValueType type,
                       SocketValue& value,
                       const std::optional<std::pair<float, float>>& range = std::nullopt) {
    switch (type) {
        case ValueType::Scalar: {
            float scalar = std::get<Scalar>(value);
            bool changed = range
                ? ImGui::SliderFloat(label, &scalar, range->first, range->second)
                : ImGui::DragFloat(label, &scalar, 0.01f);
            if (changed) {
                value = SocketValue{scalar};
            }
            return changed;
        }

        case ValueType::Vec2: {
            Vec2 vec = std::get<Vec2>(value);
            float data[2] = { vec[0], vec[1] };
            if (ImGui::DragFloat2(label, data, 0.01f)) {
                value = SocketValue{Vec2{data[0], data[1]}};
                return true;
            }
            return false;
        }

        case ValueType::Vec3: {
            Vec3 vec = std::get<Vec3>(value);
            float data[3] = { vec[0], vec[1], vec[2] };
            if (ImGui::DragFloat3(label, data, 0.01f)) {
                value = SocketValue{Vec3{data[0], data[1], data[2]}};
                return true;
            }
            return false;
        }

        case ValueType::Vec4: {
            Vec4 vec = std::get<Vec4>(value);
            float data[4] = { vec[0], vec[1], vec[2], vec[3] };
            if (ImGui::DragFloat4(label, data, 0.01f)) {
                value = SocketValue{Vec4{data[0], data[1], data[2], data[3]}};
                return true;
            }
            return false;
        }
    }

    return false;
}

std::string format_fixture_traits(const std::vector<FixtureTrait>& traits) {
    if (traits.empty()) {
        return "None";
    }

    std::string result;
    for (std::size_t i = 0; i < traits.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += std::string(fixture_trait_name(traits[i]));
    }
    return result;
}

// ============================================================================
// Application
// ============================================================================

struct PreviewProbe {
    FixtureProbe fixture;
    bool enabled = true;
};

struct PreviewProbeSample {
    uint64_t probe_id = 0;
    Vec3 world_position{0.0f, 0.0f, 0.0f};
    float scalar_value = 0.0f;
};

struct PreviewProbeRuntime {
    uint64_t probe_id = 0;
    Graph graph;
};

struct App {
    static constexpr int preview_grid_width = 16;
    static constexpr int preview_grid_height = 10;

    SDL_Window*   window     = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool          running    = true;

    Graph        graph;
    NodeId       next_id = 1;
    ConnectionId next_connection_id = 1;
    int          source_node_selection = 0;
    int          source_output_selection = 0;
    int          destination_node_selection = 0;
    int          destination_input_selection = 0;
    NodeId       preview_node_id = 0;
    int          preview_output_socket_index = 0;
    float        preview_x_min = 0.0f;
    float        preview_x_max = 1.0f;
    float        preview_y_min = 0.0f;
    float        preview_y_max = 1.0f;
    bool         show_preview_probes = true;
    uint64_t     next_preview_probe_id = 1;
    std::optional<uint64_t> selected_preview_probe_id;
    bool         preview_graphs_dirty = true;
    std::vector<Graph> preview_graphs;
    std::vector<PreviewProbeRuntime> preview_probe_graphs;
    std::vector<float> preview_samples;
    std::vector<PreviewProbe> preview_probes;
    std::vector<PreviewProbeSample> preview_probe_samples;
    std::string  last_graph_error;
    std::optional<NodeId> pending_delete_node_id;
    double       last_tick_time = 0.0;

    void tick(float dt);
    void mark_preview_dirty();
    void delete_node(NodeId id);
    bool try_add_connection(NodeId source_node_id, std::size_t source_socket_index,
                            NodeId destination_node_id, std::size_t destination_socket_index);
    void seed_default_spatial_patch();
    void seed_default_preview_probes();
    Vec2 preview_probe_center() const;
    Vec2 live_probe_position() const;
    Vec2 preview_position_for_cell(int x, int y) const;
    Vec3 preview_world_position_from_normalized(Vec2 normalized, float z = 0.0f) const;
    Vec2 preview_normalized_from_world(Vec3 position) const;
    float preview_sample_from_world(Vec3 position) const;
    PreviewProbe* find_preview_probe(uint64_t id);
    const PreviewProbe* find_preview_probe(uint64_t id) const;
    const PreviewProbeSample* find_preview_probe_sample(uint64_t id) const;
    const PreviewProbe* selected_preview_probe() const;
    std::string current_preview_output_label() const;
    void ensure_preview_probe_selection();
    std::size_t enabled_preview_probe_count() const;
    bool rebuild_preview_graphs();
    bool rebuild_preview_probe_graphs();
    bool refresh_preview_samples();
    void refresh_preview_probe_samples();
    std::optional<float> extract_preview_output(const Graph& source_graph) const;

    bool init();
    void run();
    void shutdown();

    void draw_debug_panel();
    void draw_connections_panel();
    void draw_field_preview_panel();
    void draw_node(Node& n);
    bool reset_default_patch();
    NodeId spawn_node(const char* type_name);
    NodeId spawn_node_named(const char* type_name, const std::string& instance_name);
};

bool App::init() {
    // 1. Register the current library-owned substrate primitives.
    register_builtin_node_types();

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

    seed_default_preview_probes();

    // 8. Seed a small moving scalar field so the preview is meaningful immediately.
    seed_default_spatial_patch();

    // 9. Seed the per-frame clock
    last_tick_time = (double)SDL_GetTicks() / 1000.0;

    return true;
}

void App::tick(float dt) {
    ensure_preview_probe_selection();
    set_builtin_probe_position(live_probe_position());
    GraphBuildError err;
    if (!graph.tick(dt, &err)) {
        last_graph_error = std::string(graph_build_error_name(err.code));
        fprintf(stderr, "Graph tick failed: %s\n", last_graph_error.c_str());
        running = false;
        return;
    }

    if (preview_graphs_dirty || preview_graphs.size() != (std::size_t)(preview_grid_width * preview_grid_height)) {
        if (!rebuild_preview_graphs()) {
            return;
        }
    }
    if (preview_graphs_dirty || preview_probe_graphs.size() != enabled_preview_probe_count()) {
        if (!rebuild_preview_probe_graphs()) {
            return;
        }
    }

    const Vec2 previous_probe_position = current_builtin_probe_position();
    for (int y = 0; y < preview_grid_height; ++y) {
        for (int x = 0; x < preview_grid_width; ++x) {
            const std::size_t index = (std::size_t)(y * preview_grid_width + x);
            set_builtin_probe_position(preview_position_for_cell(x, y));
            if (!preview_graphs[index].tick(dt, &err)) {
                last_graph_error = std::string(graph_build_error_name(err.code));
                fprintf(stderr, "Preview graph tick failed: %s\n", last_graph_error.c_str());
                running = false;
                set_builtin_probe_position(previous_probe_position);
                return;
            }
        }
    }
    set_builtin_probe_position(previous_probe_position);

    for (auto& probe_runtime : preview_probe_graphs) {
        const PreviewProbe* probe = find_preview_probe(probe_runtime.probe_id);
        if (!probe || !probe->enabled) {
            continue;
        }
        set_builtin_probe_position(Vec2{ probe->fixture.position[0], probe->fixture.position[1] });
        if (!probe_runtime.graph.tick(dt, &err)) {
            last_graph_error = std::string(graph_build_error_name(err.code));
            fprintf(stderr, "Preview probe graph tick failed: %s\n", last_graph_error.c_str());
            running = false;
            set_builtin_probe_position(previous_probe_position);
            return;
        }
    }
    set_builtin_probe_position(previous_probe_position);

    if (!refresh_preview_samples()) {
        running = false;
        return;
    }

    refresh_preview_probe_samples();
}

static bool rebuild_graph(Graph& graph, std::string* error_text = nullptr) {
    GraphBuildError err = graph.bake();
    if (err.code != GraphBuildErrorCode::None) {
        if (error_text) {
            *error_text = std::string(graph_build_error_name(err.code));
        }
        fprintf(stderr, "Graph bake failed: %.*s\n",
                (int)graph_build_error_name(err.code).size(),
                graph_build_error_name(err.code).data());
        return false;
    }

    if (!graph.init_pass(&err)) {
        if (error_text) {
            *error_text = std::string(graph_build_error_name(err.code));
        }
        fprintf(stderr, "Graph init pass failed: %.*s\n",
                (int)graph_build_error_name(err.code).size(),
                graph_build_error_name(err.code).data());
        return false;
    }

    if (error_text) {
        error_text->clear();
    }
    return true;
}

NodeId App::spawn_node(const char* type_name) {
    return spawn_node_named(type_name, std::string(type_name) + " #" + std::to_string(next_id));
}

NodeId App::spawn_node_named(const char* type_name, const std::string& instance_name) {
    const NodeType* t = find_node_type(type_name);
    if (!t) {
        fprintf(stderr, "spawn_node: type '%s' not found\n", type_name);
        return 0;
    }
    NodeId id = next_id++;
    graph.nodes.push_back(make_node(*t, id, instance_name));

    if (!rebuild_graph(graph, &last_graph_error)) {
        fprintf(stderr, "spawn_node: graph rebuild failed after adding '%s'\n", type_name);
        running = false;
        return 0;
    }

    mark_preview_dirty();
    return id;
}

void App::mark_preview_dirty() {
    preview_graphs_dirty = true;
}

void App::delete_node(NodeId id) {
    graph.connections.erase(
        std::remove_if(graph.connections.begin(), graph.connections.end(),
                       [id](const Connection& connection) {
                           return connection.source.node_id == id
                               || connection.destination.node_id == id;
                       }),
        graph.connections.end());

    graph.nodes.erase(
        std::remove_if(graph.nodes.begin(), graph.nodes.end(),
                       [id](const Node& node) { return node.id == id; }),
        graph.nodes.end());

    rebuild_graph(graph, &last_graph_error);
    mark_preview_dirty();
}

bool App::try_add_connection(NodeId source_node_id, std::size_t source_socket_index,
                             NodeId destination_node_id, std::size_t destination_socket_index) {
    const Connection connection{
        next_connection_id++,
        { source_node_id, source_socket_index },
        { destination_node_id, destination_socket_index },
    };

    graph.connections.push_back(connection);
    std::string failure_text;
    if (!rebuild_graph(graph, &failure_text)) {
        graph.connections.pop_back();
        rebuild_graph(graph, nullptr);
        last_graph_error = failure_text;
        return false;
    }

    last_graph_error.clear();
    mark_preview_dirty();
    return true;
}

void App::seed_default_spatial_patch() {
    const NodeId probe_x_id = spawn_node_named("ProbeX", "Probe X");
    const NodeId probe_y_id = spawn_node_named("ProbeY", "Probe Y");
    const NodeId mirror_x_id = spawn_node_named("SpatialMirror", "Mirror Symmetry");
    const NodeId frequency_y_id = spawn_node_named("Constant", "Y Frequency");
    const NodeId multiply_y_id = spawn_node_named("Multiply", "Y Offset");
    const NodeId spatial_add_id = spawn_node_named("Add", "Spatial Offset");
    const NodeId phase_id = spawn_node_named("Phase", "Sweep Phase");
    const NodeId time_offset_id = spawn_node_named("TimeOffset", "Phase Offset");
    const NodeId sine_id = spawn_node_named("Sine", "Sweep Wave");
    const NodeId decay_id = spawn_node_named("Decay", "Trail Decay");
    const NodeId background_id = spawn_node_named("Constant", "Base Level");
    const NodeId peak_id = spawn_node_named("Constant", "Peak Level");
    const NodeId mix_id = spawn_node_named("Mix", "Dimmer Mix");
    const NodeId output_dimmer_id = spawn_node_named("OutputDimmer", "Dimmer Output");
    const NodeId output_tilt_id = spawn_node_named("OutputTilt", "Tilt Output");

    if (!probe_x_id || !probe_y_id || !mirror_x_id || !frequency_y_id ||
        !multiply_y_id || !spatial_add_id || !phase_id || !time_offset_id ||
        !sine_id || !decay_id || !background_id || !peak_id || !mix_id ||
        !output_dimmer_id || !output_tilt_id) {
        return;
    }

    if (Node* frequency_y = graph.find_node(frequency_y_id)) {
        frequency_y->state["value"] = SocketValue{Scalar{0.75f}};
    }
    if (Node* phase = graph.find_node(phase_id)) {
        phase->inputs[0].default_value = SocketValue{Scalar{1.8f}};
        phase->inputs[0].current = SocketValue{Scalar{1.8f}};
    }
    if (Node* decay = graph.find_node(decay_id)) {
        decay->inputs[1].default_value = SocketValue{Scalar{0.28f}};
        decay->inputs[1].current = SocketValue{Scalar{0.28f}};
    }
    if (Node* background = graph.find_node(background_id)) {
        background->state["value"] = SocketValue{Scalar{0.08f}};
    }
    if (Node* peak = graph.find_node(peak_id)) {
        peak->state["value"] = SocketValue{Scalar{1.0f}};
    }

    if (!try_add_connection(probe_x_id, 0, mirror_x_id, 0)) return;
    if (!try_add_connection(probe_y_id, 0, multiply_y_id, 0)) return;
    if (!try_add_connection(frequency_y_id, 0, multiply_y_id, 1)) return;
    if (!try_add_connection(mirror_x_id, 0, spatial_add_id, 0)) return;
    if (!try_add_connection(multiply_y_id, 0, spatial_add_id, 1)) return;
    if (!try_add_connection(phase_id, 0, time_offset_id, 0)) return;
    if (!try_add_connection(spatial_add_id, 0, time_offset_id, 1)) return;
    if (!try_add_connection(time_offset_id, 0, sine_id, 0)) return;
    if (!try_add_connection(sine_id, 0, decay_id, 0)) return;
    if (!try_add_connection(background_id, 0, mix_id, 0)) return;
    if (!try_add_connection(peak_id, 0, mix_id, 1)) return;
    if (!try_add_connection(decay_id, 0, mix_id, 2)) return;
    if (!try_add_connection(mix_id, 0, output_dimmer_id, 0)) return;
    if (!try_add_connection(sine_id, 0, output_tilt_id, 0)) return;

    preview_node_id = output_dimmer_id;
    preview_output_socket_index = 0;
}

bool App::reset_default_patch() {
    graph = Graph{};
    next_id = 1;
    next_connection_id = 1;
    preview_node_id = 0;
    preview_output_socket_index = 0;
    source_node_selection = 0;
    source_output_selection = 0;
    destination_node_selection = 0;
    destination_input_selection = 0;
    last_graph_error.clear();
    pending_delete_node_id.reset();
    mark_preview_dirty();
    seed_default_spatial_patch();
    return running;
}

void App::seed_default_preview_probes() {
    preview_probes = {
        { FixtureProbe{ next_preview_probe_id++, "Bar L1", Vec3{0.32f, 0.15f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L2", Vec3{0.28f, 0.30f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L3", Vec3{0.24f, 0.45f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L4", Vec3{0.20f, 0.60f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L5", Vec3{0.16f, 0.75f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R1", Vec3{0.68f, 0.15f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R2", Vec3{0.72f, 0.30f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R3", Vec3{0.76f, 0.45f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R4", Vec3{0.80f, 0.60f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R5", Vec3{0.84f, 0.75f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt } }, true },
    };
    selected_preview_probe_id = preview_probes.empty()
        ? std::nullopt
        : std::optional<uint64_t>{preview_probes.front().fixture.id};
}

Vec2 App::preview_probe_center() const {
    return Vec2{
        preview_x_min + (preview_x_max - preview_x_min) * 0.5f,
        preview_y_min + (preview_y_max - preview_y_min) * 0.5f,
    };
}

Vec2 App::live_probe_position() const {
    if (const PreviewProbe* probe = selected_preview_probe()) {
        return Vec2{ probe->fixture.position[0], probe->fixture.position[1] };
    }
    return preview_probe_center();
}

Vec2 App::preview_position_for_cell(int x, int y) const {
    const float fx = preview_grid_width > 1 ? (float)x / (float)(preview_grid_width - 1) : 0.0f;
    const float fy = preview_grid_height > 1 ? (float)y / (float)(preview_grid_height - 1) : 0.0f;
    return Vec2{
        preview_x_min + fx * (preview_x_max - preview_x_min),
        preview_y_min + fy * (preview_y_max - preview_y_min),
    };
}

Vec3 App::preview_world_position_from_normalized(Vec2 normalized, float z) const {
    return Vec3{
        preview_x_min + normalized[0] * (preview_x_max - preview_x_min),
        preview_y_min + normalized[1] * (preview_y_max - preview_y_min),
        z,
    };
}

Vec2 App::preview_normalized_from_world(Vec3 position) const {
    const float x_span = std::max(preview_x_max - preview_x_min, 0.0001f);
    const float y_span = std::max(preview_y_max - preview_y_min, 0.0001f);
    return Vec2{
        (position[0] - preview_x_min) / x_span,
        (position[1] - preview_y_min) / y_span,
    };
}

float App::preview_sample_from_world(Vec3 position) const {
    if (preview_samples.empty()) {
        return 0.0f;
    }

    const Vec2 normalized = preview_normalized_from_world(position);
    const float fx = std::clamp(normalized[0], 0.0f, 1.0f);
    const float fy = std::clamp(normalized[1], 0.0f, 1.0f);
    const int x = std::clamp((int)std::lround(fx * (preview_grid_width - 1)), 0, preview_grid_width - 1);
    const int y = std::clamp((int)std::lround(fy * (preview_grid_height - 1)), 0, preview_grid_height - 1);
    return preview_samples[(std::size_t)(y * preview_grid_width + x)];
}

PreviewProbe* App::find_preview_probe(uint64_t id) {
    for (auto& probe : preview_probes) {
        if (probe.fixture.id == id) {
            return &probe;
        }
    }
    return nullptr;
}

const PreviewProbe* App::find_preview_probe(uint64_t id) const {
    for (const auto& probe : preview_probes) {
        if (probe.fixture.id == id) {
            return &probe;
        }
    }
    return nullptr;
}

const PreviewProbeSample* App::find_preview_probe_sample(uint64_t id) const {
    for (const auto& sample : preview_probe_samples) {
        if (sample.probe_id == id) {
            return &sample;
        }
    }
    return nullptr;
}

const PreviewProbe* App::selected_preview_probe() const {
    if (!selected_preview_probe_id.has_value()) {
        return nullptr;
    }
    const PreviewProbe* probe = find_preview_probe(*selected_preview_probe_id);
    if (!probe || !probe->enabled) {
        return nullptr;
    }
    return probe;
}

std::string App::current_preview_output_label() const {
    const Node* preview_node = graph.find_node(preview_node_id);
    if (!preview_node || preview_output_socket_index < 0 ||
        preview_output_socket_index >= (int)preview_node->outputs.size()) {
        return "Sample";
    }
    return preview_node->outputs[(std::size_t)preview_output_socket_index].name;
}

void App::ensure_preview_probe_selection() {
    if (selected_preview_probe()) {
        return;
    }
    for (const auto& probe : preview_probes) {
        if (probe.enabled) {
            selected_preview_probe_id = probe.fixture.id;
            return;
        }
    }
    selected_preview_probe_id.reset();
}

std::size_t App::enabled_preview_probe_count() const {
    std::size_t count = 0;
    for (const auto& probe : preview_probes) {
        if (probe.enabled) {
            ++count;
        }
    }
    return count;
}

std::optional<float> App::extract_preview_output(const Graph& source_graph) const {
    if (preview_node_id == 0) {
        return std::nullopt;
    }

    const Node* source_node = source_graph.find_node(preview_node_id);
    if (!source_node || preview_output_socket_index >= (int)source_node->outputs.size()) {
        return std::nullopt;
    }

    if (source_node->outputs[(std::size_t)preview_output_socket_index].type != ValueType::Scalar) {
        return std::nullopt;
    }

    return std::get<Scalar>(source_node->outputs[(std::size_t)preview_output_socket_index].current);
}

bool App::rebuild_preview_graphs() {
    preview_graphs.clear();
    preview_samples.assign((std::size_t)(preview_grid_width * preview_grid_height), 0.0f);
    preview_graphs.reserve((std::size_t)(preview_grid_width * preview_grid_height));

    const Vec2 previous_probe_position = current_builtin_probe_position();
    GraphBuildError err;
    for (int y = 0; y < preview_grid_height; ++y) {
        for (int x = 0; x < preview_grid_width; ++x) {
            Graph preview_graph = graph;
            preview_graph.initialized = false;
            set_builtin_probe_position(preview_position_for_cell(x, y));
            if (!preview_graph.init_pass(&err)) {
                last_graph_error = std::string(graph_build_error_name(err.code));
                fprintf(stderr, "Preview graph init failed: %s\n", last_graph_error.c_str());
                set_builtin_probe_position(previous_probe_position);
                return false;
            }
            preview_graphs.push_back(std::move(preview_graph));
        }
    }
    set_builtin_probe_position(previous_probe_position);
    return refresh_preview_samples();
}

bool App::rebuild_preview_probe_graphs() {
    preview_probe_graphs.clear();
    preview_probe_graphs.reserve(preview_probes.size());

    const Vec2 previous_probe_position = current_builtin_probe_position();
    GraphBuildError err;
    for (const auto& probe : preview_probes) {
        if (!probe.enabled) {
            continue;
        }

        PreviewProbeRuntime runtime;
        runtime.probe_id = probe.fixture.id;
        runtime.graph = graph;
        runtime.graph.initialized = false;
        set_builtin_probe_position(Vec2{ probe.fixture.position[0], probe.fixture.position[1] });
        if (!runtime.graph.init_pass(&err)) {
            last_graph_error = std::string(graph_build_error_name(err.code));
            fprintf(stderr, "Preview probe graph init failed: %s\n", last_graph_error.c_str());
            set_builtin_probe_position(previous_probe_position);
            return false;
        }
        preview_probe_graphs.push_back(std::move(runtime));
    }
    set_builtin_probe_position(previous_probe_position);
    preview_graphs_dirty = false;
    return true;
}

bool App::refresh_preview_samples() {
    if (preview_graphs.size() != (std::size_t)(preview_grid_width * preview_grid_height)) {
        return false;
    }

    for (std::size_t i = 0; i < preview_graphs.size(); ++i) {
        const std::optional<float> sample = extract_preview_output(preview_graphs[i]);
        preview_samples[i] = sample.value_or(0.0f);
    }

    return true;
}

void App::refresh_preview_probe_samples() {
    preview_probe_samples.clear();
    preview_probe_samples.reserve(preview_probe_graphs.size());

    for (const auto& runtime : preview_probe_graphs) {
        const PreviewProbe* probe = find_preview_probe(runtime.probe_id);
        if (!probe || !probe->enabled) {
            continue;
        }

        const std::optional<float> exact_sample = extract_preview_output(runtime.graph);
        preview_probe_samples.push_back(PreviewProbeSample{
            probe->fixture.id,
            probe->fixture.position,
            std::clamp(exact_sample.value_or(0.0f), 0.0f, 1.0f),
        });
    }
}

void App::draw_node(Node& n) {
    if (!ImGui::TreeNode(&n, "Node #%llu  '%s'  [%s]",
                         (unsigned long long)n.id,
                         n.name.c_str(),
                         n.type->name.c_str())) {
        return;
    }

    ImGui::Text("Position: (%.1f, %.1f)", n.position[0], n.position[1]);
    ImGui::Checkbox("Bypass", &n.bypass);
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete Node")) {
        pending_delete_node_id = n.id;
    }

    if (!n.inputs.empty()) {
        if (ImGui::TreeNode("Inputs")) {
            for (size_t i = 0; i < n.inputs.size(); ++i) {
                Socket& s = n.inputs[i];
                ImGui::PushID((int)i);
                ImGui::Text("[%zu] %s (%s)", i, s.name.c_str(), value_type_name(s.type));
                SocketValue editable = s.default_value;
                if (edit_value_widget("Default", s.type, editable, s.range)) {
                    s.default_value = editable;
                    s.current = editable;
                    mark_preview_dirty();
                }
                ImGui::Text("Current: %s", format_value(s.current).c_str());
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    if (!n.outputs.empty()) {
        if (ImGui::TreeNode("Outputs")) {
            for (size_t i = 0; i < n.outputs.size(); ++i) {
                const Socket& s = n.outputs[i];
                ImGui::Text("[%zu] %s (%s) = %s",
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
                ImGui::PushID(key.c_str());
                ImGui::Text("%s (%s)", key.c_str(), value_type_name(vt));
                SocketValue editable = val;
                if (edit_value_widget("Value", vt, editable)) {
                    n.state[key] = editable;
                    mark_preview_dirty();
                }
                ImGui::Text("Current: %s", format_value(n.state.at(key)).c_str());
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    ImGui::TreePop();
}

void App::draw_connections_panel() {
    if (!ImGui::CollapsingHeader("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("Count: %zu", graph.connections.size());
    if (!last_graph_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Last graph error: %s", last_graph_error.c_str());
    }

    for (std::size_t i = 0; i < graph.connections.size(); ++i) {
        const auto& connection = graph.connections[i];
        const Node* src_node = graph.find_node(connection.source.node_id);
        const Node* dst_node = graph.find_node(connection.destination.node_id);
        const char* src_socket = (src_node && connection.source.socket_index < src_node->outputs.size())
            ? src_node->outputs[connection.source.socket_index].name.c_str()
            : "?";
        const char* dst_socket = (dst_node && connection.destination.socket_index < dst_node->inputs.size())
            ? dst_node->inputs[connection.destination.socket_index].name.c_str()
            : "?";

        ImGui::PushID((int)connection.id);
        ImGui::Text("#%llu  %s.%s -> %s.%s",
                    (unsigned long long)connection.id,
                    src_node ? src_node->name.c_str() : "?",
                    src_socket,
                    dst_node ? dst_node->name.c_str() : "?",
                    dst_socket);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            graph.connections.erase(graph.connections.begin() + (long)i);
            rebuild_graph(graph, &last_graph_error);
            mark_preview_dirty();
            ImGui::PopID();
            return;
        }
        ImGui::PopID();
    }

    std::vector<int> source_nodes;
    std::vector<int> destination_nodes;
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        if (!graph.nodes[i].outputs.empty()) {
            source_nodes.push_back((int)i);
        }
        if (!graph.nodes[i].inputs.empty()) {
            destination_nodes.push_back((int)i);
        }
    }

    if (source_nodes.empty() || destination_nodes.empty()) {
        ImGui::TextUnformatted("Need at least one source node with outputs and one destination node with inputs.");
        return;
    }

    if (source_node_selection >= (int)source_nodes.size()) {
        source_node_selection = 0;
    }
    if (destination_node_selection >= (int)destination_nodes.size()) {
        destination_node_selection = 0;
    }

    Node& source_node = graph.nodes[(std::size_t)source_nodes[(std::size_t)source_node_selection]];
    Node& destination_node = graph.nodes[(std::size_t)destination_nodes[(std::size_t)destination_node_selection]];

    if (source_output_selection >= (int)source_node.outputs.size()) {
        source_output_selection = 0;
    }
    if (destination_input_selection >= (int)destination_node.inputs.size()) {
        destination_input_selection = 0;
    }

    if (ImGui::BeginCombo("Source Node", source_node.name.c_str())) {
        for (std::size_t i = 0; i < source_nodes.size(); ++i) {
            const bool selected = ((int)i == source_node_selection);
            const auto& node = graph.nodes[(std::size_t)source_nodes[i]];
            if (ImGui::Selectable(node.name.c_str(), selected)) {
                source_node_selection = (int)i;
                source_output_selection = 0;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("Source Output", source_node.outputs[(std::size_t)source_output_selection].name.c_str())) {
        for (std::size_t i = 0; i < source_node.outputs.size(); ++i) {
            const bool selected = ((int)i == source_output_selection);
            if (ImGui::Selectable(source_node.outputs[i].name.c_str(), selected)) {
                source_output_selection = (int)i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("Destination Node", destination_node.name.c_str())) {
        for (std::size_t i = 0; i < destination_nodes.size(); ++i) {
            const bool selected = ((int)i == destination_node_selection);
            const auto& node = graph.nodes[(std::size_t)destination_nodes[i]];
            if (ImGui::Selectable(node.name.c_str(), selected)) {
                destination_node_selection = (int)i;
                destination_input_selection = 0;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("Destination Input", destination_node.inputs[(std::size_t)destination_input_selection].name.c_str())) {
        for (std::size_t i = 0; i < destination_node.inputs.size(); ++i) {
            const bool selected = ((int)i == destination_input_selection);
            if (ImGui::Selectable(destination_node.inputs[i].name.c_str(), selected)) {
                destination_input_selection = (int)i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Add Connection")) {
        try_add_connection(source_node.id, (std::size_t)source_output_selection,
                           destination_node.id, (std::size_t)destination_input_selection);
    }
}

void App::draw_field_preview_panel() {
    if (!ImGui::CollapsingHeader("Field Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ensure_preview_probe_selection();

    std::vector<const Node*> scalar_output_nodes;
    for (const auto& node : graph.nodes) {
        for (const auto& output : node.outputs) {
            if (output.type == ValueType::Scalar) {
                scalar_output_nodes.push_back(&node);
                break;
            }
        }
    }

    if (scalar_output_nodes.empty()) {
        ImGui::TextUnformatted("Need at least one node with a scalar output.");
        return;
    }

    if (!graph.find_node(preview_node_id)) {
        preview_node_id = scalar_output_nodes.front()->id;
        preview_output_socket_index = 0;
    }

    const Node* preview_node = graph.find_node(preview_node_id);
    if (!preview_node) {
        ImGui::TextUnformatted("Preview target is unavailable.");
        return;
    }

    if (ImGui::BeginCombo("Preview Node", preview_node->name.c_str())) {
        for (const Node* node : scalar_output_nodes) {
            const bool selected = node->id == preview_node_id;
            if (ImGui::Selectable(node->name.c_str(), selected)) {
                preview_node_id = node->id;
                preview_output_socket_index = 0;
                preview_node = node;
                refresh_preview_samples();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    std::vector<std::size_t> scalar_outputs;
    for (std::size_t i = 0; i < preview_node->outputs.size(); ++i) {
        if (preview_node->outputs[i].type == ValueType::Scalar) {
            scalar_outputs.push_back(i);
        }
    }

    if (scalar_outputs.empty()) {
        ImGui::TextUnformatted("Selected node has no scalar outputs.");
        return;
    }

    if (std::find(scalar_outputs.begin(), scalar_outputs.end(),
                  (std::size_t)preview_output_socket_index) == scalar_outputs.end()) {
        preview_output_socket_index = (int)scalar_outputs.front();
    }

    if (ImGui::BeginCombo("Preview Output", preview_node->outputs[(std::size_t)preview_output_socket_index].name.c_str())) {
        for (const std::size_t output_index : scalar_outputs) {
            const bool selected = ((int)output_index == preview_output_socket_index);
            if (ImGui::Selectable(preview_node->outputs[output_index].name.c_str(), selected)) {
                preview_output_socket_index = (int)output_index;
                refresh_preview_samples();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto normalize_bounds = [](float& min_value, float& max_value) {
        if (min_value > max_value) {
            std::swap(min_value, max_value);
        }
        if ((max_value - min_value) < 0.001f) {
            max_value = min_value + 0.001f;
        }
    };

    float x_bounds[2] = { preview_x_min, preview_x_max };
    if (ImGui::DragFloat2("X Bounds", x_bounds, 0.01f)) {
        preview_x_min = x_bounds[0];
        preview_x_max = x_bounds[1];
        normalize_bounds(preview_x_min, preview_x_max);
        mark_preview_dirty();
    }

    float y_bounds[2] = { preview_y_min, preview_y_max };
    if (ImGui::DragFloat2("Y Bounds", y_bounds, 0.01f)) {
        preview_y_min = y_bounds[0];
        preview_y_max = y_bounds[1];
        normalize_bounds(preview_y_min, preview_y_max);
        mark_preview_dirty();
    }

    if (ImGui::Button("Reset Domain")) {
        preview_x_min = 0.0f;
        preview_x_max = 1.0f;
        preview_y_min = 0.0f;
        preview_y_max = 1.0f;
        mark_preview_dirty();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show Probe Overlay", &show_preview_probes);

    if (preview_graphs_dirty) {
        if (!rebuild_preview_graphs()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                               "Preview rebuild failed: %s", last_graph_error.c_str());
            return;
        }
    }
    if (preview_probe_samples.empty() && !preview_samples.empty()) {
        refresh_preview_probe_samples();
    }

    constexpr float cell_size = 22.0f;
    constexpr float cell_padding = 3.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (int y = 0; y < preview_grid_height; ++y) {
        for (int x = 0; x < preview_grid_width; ++x) {
            const std::size_t index = (std::size_t)(y * preview_grid_width + x);
            const float value = std::clamp(preview_samples[index], 0.0f, 1.0f);

            const ImVec2 p1{
                origin.x + x * (cell_size + cell_padding),
                origin.y + y * (cell_size + cell_padding),
            };
            const ImVec2 p2{ p1.x + cell_size, p1.y + cell_size };
            const ImU32 color = ImGui::GetColorU32(ImVec4(value, value * 0.8f, 0.15f + value * 0.85f, 1.0f));

            draw_list->AddRectFilled(p1, p2, color, 4.0f);
            draw_list->AddRect(p1, p2, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 1.0f)), 4.0f);
        }
    }

    if (show_preview_probes) {
        for (const auto& probe : preview_probes) {
            if (!probe.enabled) {
                continue;
            }
            const Vec2 normalized = preview_normalized_from_world(probe.fixture.position);
            const PreviewProbeSample* exact_sample = find_preview_probe_sample(probe.fixture.id);
            const float sample = std::clamp(exact_sample ? exact_sample->scalar_value : 0.0f, 0.0f, 1.0f);
            const ImVec2 center{
                origin.x + normalized[0] * ((preview_grid_width - 1) * (cell_size + cell_padding)),
                origin.y + normalized[1] * ((preview_grid_height - 1) * (cell_size + cell_padding)),
            };
            const bool selected = selected_preview_probe_id.has_value() && *selected_preview_probe_id == probe.fixture.id;
            const ImU32 ring_color = ImGui::GetColorU32(
                selected ? ImVec4(1.0f, 0.95f, 0.35f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
            const ImU32 fill_color = ImGui::GetColorU32(ImVec4(sample, 0.25f + sample * 0.75f, 0.2f, 1.0f));
            draw_list->AddCircleFilled(center, 5.0f, fill_color);
            draw_list->AddCircle(center, 7.0f, ring_color, 0, 2.0f);

            char label[32];
            std::snprintf(label, sizeof(label), "%s %.2f", probe.fixture.name.c_str(), sample);
            draw_list->AddText(ImVec2(center.x + 8.0f, center.y - 8.0f),
                               ring_color,
                               label);
        }
    }

    ImGui::Dummy(ImVec2(
        preview_grid_width * (cell_size + cell_padding),
        preview_grid_height * (cell_size + cell_padding)));
    const Vec2 center = preview_probe_center();
    ImGui::Text("Domain: X %.2f..%.2f, Y %.2f..%.2f",
                preview_x_min, preview_x_max, preview_y_min, preview_y_max);
    if (const PreviewProbe* selected_probe = selected_preview_probe()) {
        ImGui::Text("Selected probe: %s at [%.2f, %.2f, %.2f]",
                    selected_probe->fixture.name.c_str(),
                    selected_probe->fixture.position[0],
                    selected_probe->fixture.position[1],
                    selected_probe->fixture.position[2]);
    } else {
        ImGui::Text("Inspector probe center: [%.2f, %.2f]", center[0], center[1]);
    }
    if (show_preview_probes) {
        std::size_t enabled_count = 0;
        for (const auto& probe : preview_probes) {
            if (probe.enabled) {
                ++enabled_count;
            }
        }
        ImGui::Text("Probe overlay: %zu enabled sample points shown over the preview field.", enabled_count);
    }
    ImGui::Text("Prototype preview: one persistent graph state per sampled probe.");

    if (ImGui::TreeNode("Preview Probes")) {
        if (ImGui::Button("Add Probe")) {
            const std::size_t next_index = preview_probes.size() + 1;
            preview_probes.push_back(PreviewProbe{
                FixtureProbe{
                    next_preview_probe_id++,
                    "Probe " + std::to_string(next_index),
                    preview_world_position_from_normalized(Vec2{0.5f, 0.5f}),
                    { FixtureTrait::Dimmer },
                },
                true,
            });
            selected_preview_probe_id = preview_probes.back().fixture.id;
            mark_preview_dirty();
        }

        std::optional<uint64_t> probe_to_delete;
        for (auto& probe : preview_probes) {
            ImGui::PushID((int)probe.fixture.id);

            const bool selected = selected_preview_probe_id.has_value() && *selected_preview_probe_id == probe.fixture.id;
            if (ImGui::Selectable(probe.fixture.name.c_str(), selected)) {
                selected_preview_probe_id = probe.fixture.id;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Enabled", &probe.enabled)) {
                mark_preview_dirty();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                probe_to_delete = probe.fixture.id;
            }

            char name_buffer[64];
            std::snprintf(name_buffer, sizeof(name_buffer), "%s", probe.fixture.name.c_str());
            if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
                probe.fixture.name = name_buffer;
            }

            float world_xyz[3] = {
                probe.fixture.position[0],
                probe.fixture.position[1],
                probe.fixture.position[2],
            };
            if (ImGui::DragFloat3("World XYZ", world_xyz, 0.01f)) {
                probe.fixture.position = Vec3{world_xyz[0], world_xyz[1], world_xyz[2]};
                mark_preview_dirty();
            }

            bool dimmer = fixture_has_trait(probe.fixture, FixtureTrait::Dimmer);
            if (ImGui::Checkbox("Dimmer Trait", &dimmer)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Dimmer, dimmer);
            }
            ImGui::SameLine();
            bool pan = fixture_has_trait(probe.fixture, FixtureTrait::Pan);
            if (ImGui::Checkbox("Pan Trait", &pan)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Pan, pan);
            }
            ImGui::SameLine();
            bool tilt = fixture_has_trait(probe.fixture, FixtureTrait::Tilt);
            if (ImGui::Checkbox("Tilt Trait", &tilt)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Tilt, tilt);
            }
            ImGui::SameLine();
            bool color_rgb = fixture_has_trait(probe.fixture, FixtureTrait::ColorRGB);
            if (ImGui::Checkbox("ColorRGB Trait", &color_rgb)) {
                fixture_set_trait(probe.fixture, FixtureTrait::ColorRGB, color_rgb);
            }

            const float sample = std::clamp(preview_sample_from_world(probe.fixture.position), 0.0f, 1.0f);
            ImGui::Text("Traits: %s", format_fixture_traits(probe.fixture.traits).c_str());
            ImGui::Text("World XYZ: [%.2f, %.2f, %.2f]  Sample: %.2f",
                        probe.fixture.position[0],
                        probe.fixture.position[1],
                        probe.fixture.position[2],
                        sample);
            ImGui::Separator();
            ImGui::PopID();
        }

        if (probe_to_delete.has_value()) {
            preview_probes.erase(
                std::remove_if(preview_probes.begin(), preview_probes.end(),
                               [&](const PreviewProbe& probe) { return probe.fixture.id == *probe_to_delete; }),
                preview_probes.end());
            if (selected_preview_probe_id.has_value() && *selected_preview_probe_id == *probe_to_delete) {
                selected_preview_probe_id.reset();
            }
            mark_preview_dirty();
            ensure_preview_probe_selection();
        } else {
            ensure_preview_probe_selection();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Sampled Points")) {
        if (preview_probe_samples.empty()) {
            ImGui::TextUnformatted("No enabled sample points.");
        } else {
            for (const auto& sample : preview_probe_samples) {
                const PreviewProbe* probe = find_preview_probe(sample.probe_id);
                if (!probe) {
                    continue;
                }

                ImGui::PushID((int)sample.probe_id);
                const bool selected = selected_preview_probe_id.has_value() && *selected_preview_probe_id == sample.probe_id;
                if (ImGui::Selectable(probe->fixture.name.c_str(), selected)) {
                    selected_preview_probe_id = sample.probe_id;
                }
                ImGui::Text("ID: %llu  XYZ: [%.2f, %.2f, %.2f]",
                            (unsigned long long)sample.probe_id,
                            sample.world_position[0],
                            sample.world_position[1],
                            sample.world_position[2]);
                ImGui::Text("Traits: %s", format_fixture_traits(probe->fixture.traits).c_str());
                const std::string sample_label = current_preview_output_label();
                ImGui::ProgressBar(sample.scalar_value, ImVec2(-1.0f, 0.0f), sample_label.c_str());
                ImGui::Text("%s sample: %.3f", sample_label.c_str(), sample.scalar_value);
                ImGui::Separator();
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
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
    if (ImGui::Button("Reset Diagonal Sweep")) {
        reset_default_patch();
    }
    ImGui::SameLine();
    if (ImGui::Button("Constant"))      spawn_node("Constant");
    ImGui::SameLine();
    if (ImGui::Button("Constant Vec3")) spawn_node("ConstantVec3");
    ImGui::SameLine();
    if (ImGui::Button("Phase"))         spawn_node("Phase");
    ImGui::SameLine();
    if (ImGui::Button("Add"))           spawn_node("Add");
    ImGui::SameLine();
    if (ImGui::Button("Multiply"))      spawn_node("Multiply");
    ImGui::SameLine();
    if (ImGui::Button("Mix"))           spawn_node("Mix");
    ImGui::SameLine();
    if (ImGui::Button("Output Dimmer")) spawn_node("OutputDimmer");
    ImGui::SameLine();
    if (ImGui::Button("Output Tilt"))   spawn_node("OutputTilt");
    ImGui::SameLine();
    if (ImGui::Button("Sine"))          spawn_node("Sine");
    ImGui::SameLine();
    if (ImGui::Button("Time Offset"))   spawn_node("TimeOffset");
    ImGui::SameLine();
    if (ImGui::Button("Decay"))         spawn_node("Decay");
    ImGui::SameLine();
    if (ImGui::Button("ProbeX"))        spawn_node("ProbeX");
    ImGui::SameLine();
    if (ImGui::Button("ProbeY"))        spawn_node("ProbeY");
    ImGui::SameLine();
    if (ImGui::Button("Spatial Mirror")) spawn_node("SpatialMirror");

    ImGui::Separator();
    draw_connections_panel();

    ImGui::Separator();
    draw_field_preview_panel();

    // Demo nodes
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Count: %zu", graph.nodes.size());
        for (auto& n : graph.nodes) {
            draw_node(n);
        }
    }

    if (pending_delete_node_id.has_value()) {
        delete_node(*pending_delete_node_id);
        pending_delete_node_id.reset();
    }

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Elapsed: %.3f s", graph.elapsed_seconds);
    ImGui::Text("Bake state: %s", graph.bake_state == GraphBakeState::Baked ? "Baked" : "Editing");
    ImGui::Text("Connections: %zu", graph.connections.size());
    ImGui::Text("Eval order nodes: %zu", graph.evaluation_order.size());
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

        // Compute dt and tick the substrate graph.
        double now = (double)SDL_GetTicks() / 1000.0;
        float  dt  = (float)(now - last_tick_time);
        last_tick_time = now;
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
