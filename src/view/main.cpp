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
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
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

std::optional<std::string> mirrored_probe_name(std::string_view name) {
    constexpr std::string_view left_prefix = "Bar L";
    constexpr std::string_view right_prefix = "Bar R";

    if (name.substr(0, left_prefix.size()) == left_prefix) {
        return std::string(right_prefix) + std::string(name.substr(left_prefix.size()));
    }
    if (name.substr(0, right_prefix.size()) == right_prefix) {
        return std::string(left_prefix) + std::string(name.substr(right_prefix.size()));
    }
    return std::nullopt;
}

enum class PreviewPlane {
    XY,
    XZ,
    YZ,
};

const char* preview_plane_name(PreviewPlane plane) {
    switch (plane) {
        case PreviewPlane::XY: return "XY";
        case PreviewPlane::XZ: return "XZ";
        case PreviewPlane::YZ: return "YZ";
    }
    return "XY";
}

const char* world_axis_name(int axis) {
    switch (axis) {
        case 0: return "X";
        case 1: return "Y";
        case 2: return "Z";
    }
    return "?";
}

std::array<int, 2> preview_plane_axes(PreviewPlane plane) {
    switch (plane) {
        case PreviewPlane::XY: return {0, 1};
        case PreviewPlane::XZ: return {0, 2};
        case PreviewPlane::YZ: return {1, 2};
    }
    return {0, 1};
}

int preview_plane_slice_axis(PreviewPlane plane) {
    switch (plane) {
        case PreviewPlane::XY: return 2;
        case PreviewPlane::XZ: return 1;
        case PreviewPlane::YZ: return 0;
    }
    return 2;
}

float world_axis_value(Vec3 position, int axis) {
    return position[(std::size_t)axis];
}

void set_world_axis_value(Vec3& position, int axis, float value) {
    position[(std::size_t)axis] = value;
}

// ============================================================================
// Application
// ============================================================================

struct PreviewProbe {
    FixtureProbe fixture;
    bool enabled = true;
};

struct PreviewOutputSample {
    std::optional<float> scalar_value;
    std::optional<Vec3> color_value;
};

struct PreviewProbeSample {
    uint64_t probe_id = 0;
    Vec3 world_position{0.0f, 0.0f, 0.0f};
    std::optional<float> preview_scalar_value;
    std::optional<Vec3> preview_color_value;
    std::optional<float> dimmer_value;
    std::optional<float> tilt_value;
    std::optional<Vec3> driver_color_value;
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
    char         node_spawn_filter[64] = "";
    int          filtered_spawn_selection = 0;
    bool         focus_node_spawn_filter = false;
    NodeId       preview_node_id = 0;
    NodeId       spatial_fixture_driver_node_id = 0;
    int          preview_output_socket_index = 0;
    PreviewPlane preview_plane = PreviewPlane::XY;
    float        preview_u_min = 0.0f;
    float        preview_u_max = 1.0f;
    float        preview_v_min = 0.0f;
    float        preview_v_max = 1.0f;
    float        preview_slice = 0.0f;
    bool         show_preview_probes = true;
    bool         lock_mirrored_preview_pairs = true;
    uint64_t     next_preview_probe_id = 1;
    std::optional<uint64_t> selected_preview_probe_id;
    std::optional<uint64_t> dragged_preview_probe_id;
    bool         preview_graphs_dirty = true;
    std::vector<Graph> preview_graphs;
    std::vector<PreviewProbeRuntime> preview_probe_graphs;
    std::vector<PreviewOutputSample> preview_samples;
    std::vector<PreviewProbe> preview_probes;
    std::vector<PreviewProbeSample> preview_probe_samples;
    std::unordered_map<NodeId, std::deque<double>> bpm_tap_history_seconds;
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
    Vec3 preview_probe_center() const;
    Vec3 live_probe_position() const;
    Vec3 preview_position_for_cell(int x, int y) const;
    Vec3 preview_world_position_from_normalized(Vec2 normalized, float z = 0.0f) const;
    Vec2 preview_normalized_from_world(Vec3 position) const;
    bool fit_preview_domain_to_probes(float padding_ratio = 0.08f);
    float preview_probe_mirror_center_x() const;
    PreviewOutputSample preview_sample_from_world(Vec3 position) const;
    PreviewProbe* find_preview_probe(uint64_t id);
    const PreviewProbe* find_preview_probe(uint64_t id) const;
    PreviewProbe* find_preview_probe_by_name(std::string_view name);
    const PreviewProbe* find_preview_probe_by_name(std::string_view name) const;
    const PreviewProbeSample* find_preview_probe_sample(uint64_t id) const;
    const PreviewProbe* selected_preview_probe() const;
    bool sync_mirrored_preview_probe(const PreviewProbe& source_probe, bool copy_traits);
    std::string current_preview_output_label() const;
    std::optional<ValueType> current_preview_output_type() const;
    void ensure_preview_probe_selection();
    std::size_t enabled_preview_probe_count() const;
    bool rebuild_preview_graphs();
    bool rebuild_preview_probe_graphs();
    bool refresh_preview_samples();
    void refresh_preview_probe_samples();
    void clear_bpm_tap_history(NodeId id);
    void tap_bpm(Node& node);
    PreviewOutputSample extract_preview_output(const Graph& source_graph) const;
    std::optional<float> extract_node_output_scalar(const Graph& source_graph, NodeId node_id, std::size_t output_index) const;
    std::optional<Vec3> extract_node_output_vec3(const Graph& source_graph, NodeId node_id, std::size_t output_index) const;

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

    const Vec3 previous_probe_position = current_builtin_probe_position();
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
        set_builtin_probe_position(probe->fixture.position);
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

void App::clear_bpm_tap_history(NodeId id) {
    bpm_tap_history_seconds.erase(id);
}

void App::tap_bpm(Node& node) {
    if (!node.type || node.type->name != "BPMTap") {
        return;
    }

    auto& tap_times = bpm_tap_history_seconds[node.id];
    const double now_seconds = ImGui::GetTime();
    if (!tap_times.empty() && (now_seconds - tap_times.back()) > 2.5) {
        tap_times.clear();
    }

    tap_times.push_back(now_seconds);
    while (tap_times.size() > 5) {
        tap_times.pop_front();
    }

    if (tap_times.size() < 2) {
        return;
    }

    double interval_sum = 0.0;
    std::size_t interval_count = 0;
    for (std::size_t i = 1; i < tap_times.size(); ++i) {
        const double interval = tap_times[i] - tap_times[i - 1];
        if (interval < 0.2 || interval > 3.0) {
            continue;
        }
        interval_sum += interval;
        ++interval_count;
    }

    if (interval_count == 0) {
        return;
    }

    const float bpm = std::clamp((float)(60.0 / (interval_sum / (double)interval_count)),
                                 20.0f, 300.0f);
    node.state["bpm"] = SocketValue{Scalar{bpm}};
    node.type->evaluate(node, 0.0f, 0.0f, true);
    mark_preview_dirty();
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

    if (preview_node_id == id) {
        preview_node_id = 0;
        preview_output_socket_index = 0;
    }
    if (spatial_fixture_driver_node_id == id) {
        spatial_fixture_driver_node_id = 0;
    }
    clear_bpm_tap_history(id);

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
    const NodeId probe_z_id = spawn_node_named("ProbeZ", "Probe Z");
    const NodeId mirror_x_id = spawn_node_named("SpatialMirror", "Mirror Symmetry");
    const NodeId project3d_id = spawn_node_named("Project3D", "Bar Coordinate");
    const NodeId bpm_tap_id = spawn_node_named("BPMTap", "Sweep Tempo");
    const NodeId beats_per_sweep_id = spawn_node_named("Constant", "Beats Per Sweep");
    const NodeId sweep_period_id = spawn_node_named("Multiply", "Sweep Period");
    const NodeId phase_id = spawn_node_named("Phase", "Sweep Phase");
    const NodeId time_offset_id = spawn_node_named("TimeOffset", "Phase Offset");
    const NodeId ramp_id = spawn_node_named("Ramp", "Sweep Ramp");
    const NodeId decay_id = spawn_node_named("Decay", "Trail Decay");
    const NodeId base_tilt_id = spawn_node_named("Constant", "Base Tilt");
    const NodeId peak_tilt_id = spawn_node_named("Constant", "Peak Tilt");
    const NodeId tilt_mix_id = spawn_node_named("Mix", "Tilt Mix");
    const NodeId full_intensity_id = spawn_node_named("Constant", "Full Intensity");
    const NodeId white_id = spawn_node_named("ConstantVec3", "Base White");
    const NodeId red_id = spawn_node_named("ConstantVec3", "Sweep Red");
    const NodeId color_mix_id = spawn_node_named("MixVec3", "Color Mix");
    const NodeId fixture_driver_id = spawn_node_named("SpatialFixtureDriver", "Fixture Driver");

    if (!probe_x_id || !probe_y_id || !probe_z_id || !mirror_x_id || !project3d_id ||
        !bpm_tap_id || !beats_per_sweep_id ||
        !sweep_period_id || !phase_id || !time_offset_id ||
        !ramp_id || !decay_id || !base_tilt_id || !peak_tilt_id || !tilt_mix_id ||
        !full_intensity_id ||
        !white_id || !red_id || !color_mix_id || !fixture_driver_id) {
        return;
    }

    if (Node* project3d = graph.find_node(project3d_id)) {
        project3d->inputs[3].default_value = SocketValue{Scalar{0.47f}};
        project3d->inputs[3].current = SocketValue{Scalar{0.47f}};
        project3d->inputs[4].default_value = SocketValue{Scalar{0.88f}};
        project3d->inputs[4].current = SocketValue{Scalar{0.88f}};
        project3d->inputs[5].default_value = SocketValue{Scalar{0.18f}};
        project3d->inputs[5].current = SocketValue{Scalar{0.18f}};
    }
    if (Node* bpm_tap = graph.find_node(bpm_tap_id)) {
        bpm_tap->state["bpm"] = SocketValue{Scalar{100.0f}};
    }
    if (Node* beats_per_sweep = graph.find_node(beats_per_sweep_id)) {
        beats_per_sweep->state["value"] = SocketValue{Scalar{3.0f}};
    }
    if (Node* decay = graph.find_node(decay_id)) {
        decay->inputs[1].default_value = SocketValue{Scalar{0.28f}};
        decay->inputs[1].current = SocketValue{Scalar{0.28f}};
    }
    if (Node* base_tilt = graph.find_node(base_tilt_id)) {
        base_tilt->state["value"] = SocketValue{Scalar{0.30f}};
    }
    if (Node* peak_tilt = graph.find_node(peak_tilt_id)) {
        peak_tilt->state["value"] = SocketValue{Scalar{0.82f}};
    }
    if (Node* full_intensity = graph.find_node(full_intensity_id)) {
        full_intensity->state["value"] = SocketValue{Scalar{1.0f}};
    }
    if (Node* white = graph.find_node(white_id)) {
        white->state["value"] = SocketValue{Vec3{1.0f, 1.0f, 1.0f}};
    }
    if (Node* red = graph.find_node(red_id)) {
        red->state["value"] = SocketValue{Vec3{1.0f, 0.0f, 0.0f}};
    }

    if (!try_add_connection(probe_x_id, 0, mirror_x_id, 0)) return;
    if (!try_add_connection(mirror_x_id, 0, project3d_id, 0)) return;
    if (!try_add_connection(probe_y_id, 0, project3d_id, 1)) return;
    if (!try_add_connection(probe_z_id, 0, project3d_id, 2)) return;
    if (!try_add_connection(bpm_tap_id, 1, sweep_period_id, 0)) return;
    if (!try_add_connection(beats_per_sweep_id, 0, sweep_period_id, 1)) return;
    if (!try_add_connection(sweep_period_id, 0, phase_id, 0)) return;
    if (!try_add_connection(phase_id, 0, time_offset_id, 0)) return;
    if (!try_add_connection(project3d_id, 0, time_offset_id, 1)) return;
    if (!try_add_connection(time_offset_id, 0, ramp_id, 0)) return;
    if (!try_add_connection(ramp_id, 0, decay_id, 0)) return;
    if (!try_add_connection(base_tilt_id, 0, tilt_mix_id, 0)) return;
    if (!try_add_connection(peak_tilt_id, 0, tilt_mix_id, 1)) return;
    if (!try_add_connection(decay_id, 0, tilt_mix_id, 2)) return;
    if (!try_add_connection(white_id, 0, color_mix_id, 0)) return;
    if (!try_add_connection(red_id, 0, color_mix_id, 1)) return;
    if (!try_add_connection(decay_id, 0, color_mix_id, 2)) return;
    if (!try_add_connection(full_intensity_id, 0, fixture_driver_id, 0)) return;
    if (!try_add_connection(tilt_mix_id, 0, fixture_driver_id, 1)) return;
    if (!try_add_connection(color_mix_id, 0, fixture_driver_id, 2)) return;

    spatial_fixture_driver_node_id = fixture_driver_id;
    preview_node_id = fixture_driver_id;
    preview_output_socket_index = 2;
}

bool App::reset_default_patch() {
    graph = Graph{};
    next_id = 1;
    next_connection_id = 1;
    bpm_tap_history_seconds.clear();
    preview_node_id = 0;
    spatial_fixture_driver_node_id = 0;
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
        { FixtureProbe{ next_preview_probe_id++, "Bar L1", Vec3{0.32f, 0.15f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L2", Vec3{0.28f, 0.30f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L3", Vec3{0.24f, 0.45f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L4", Vec3{0.20f, 0.60f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar L5", Vec3{0.16f, 0.75f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R1", Vec3{0.68f, 0.15f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R2", Vec3{0.72f, 0.30f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R3", Vec3{0.76f, 0.45f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R4", Vec3{0.80f, 0.60f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
        { FixtureProbe{ next_preview_probe_id++, "Bar R5", Vec3{0.84f, 0.75f, 0.0f}, { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB } }, true },
    };
    selected_preview_probe_id = preview_probes.empty()
        ? std::nullopt
        : std::optional<uint64_t>{preview_probes.front().fixture.id};
}

Vec3 App::preview_probe_center() const {
    return preview_world_position_from_normalized(Vec2{0.5f, 0.5f}, preview_slice);
}

Vec3 App::live_probe_position() const {
    if (const PreviewProbe* probe = selected_preview_probe()) {
        return probe->fixture.position;
    }
    return preview_probe_center();
}

Vec3 App::preview_position_for_cell(int x, int y) const {
    const float fx = preview_grid_width > 1 ? (float)x / (float)(preview_grid_width - 1) : 0.0f;
    const float fy = preview_grid_height > 1 ? (float)y / (float)(preview_grid_height - 1) : 0.0f;
    return preview_world_position_from_normalized(Vec2{fx, fy}, preview_slice);
}

Vec3 App::preview_world_position_from_normalized(Vec2 normalized, float slice_value) const {
    const auto axes = preview_plane_axes(preview_plane);
    Vec3 position{0.0f, 0.0f, 0.0f};
    set_world_axis_value(position, axes[0], preview_u_min + normalized[0] * (preview_u_max - preview_u_min));
    set_world_axis_value(position, axes[1], preview_v_min + normalized[1] * (preview_v_max - preview_v_min));
    set_world_axis_value(position, preview_plane_slice_axis(preview_plane), slice_value);
    return position;
}

Vec2 App::preview_normalized_from_world(Vec3 position) const {
    const auto axes = preview_plane_axes(preview_plane);
    const float u_span = std::max(preview_u_max - preview_u_min, 0.0001f);
    const float v_span = std::max(preview_v_max - preview_v_min, 0.0001f);
    return Vec2{
        (world_axis_value(position, axes[0]) - preview_u_min) / u_span,
        (world_axis_value(position, axes[1]) - preview_v_min) / v_span,
    };
}

bool App::fit_preview_domain_to_probes(float padding_ratio) {
    const PreviewProbe* first_enabled_probe = nullptr;
    for (const auto& probe : preview_probes) {
        if (probe.enabled) {
            first_enabled_probe = &probe;
            break;
        }
    }

    if (!first_enabled_probe) {
        return false;
    }

    const auto axes = preview_plane_axes(preview_plane);
    float min_u = world_axis_value(first_enabled_probe->fixture.position, axes[0]);
    float max_u = min_u;
    float min_v = world_axis_value(first_enabled_probe->fixture.position, axes[1]);
    float max_v = min_v;
    for (const auto& probe : preview_probes) {
        if (!probe.enabled) {
            continue;
        }
        min_u = std::min(min_u, world_axis_value(probe.fixture.position, axes[0]));
        max_u = std::max(max_u, world_axis_value(probe.fixture.position, axes[0]));
        min_v = std::min(min_v, world_axis_value(probe.fixture.position, axes[1]));
        max_v = std::max(max_v, world_axis_value(probe.fixture.position, axes[1]));
    }

    const float u_span = std::max(max_u - min_u, 0.1f);
    const float v_span = std::max(max_v - min_v, 0.1f);
    const float u_padding = u_span * std::max(padding_ratio, 0.0f);
    const float v_padding = v_span * std::max(padding_ratio, 0.0f);

    preview_u_min = min_u - u_padding;
    preview_u_max = max_u + u_padding;
    preview_v_min = min_v - v_padding;
    preview_v_max = max_v + v_padding;
    mark_preview_dirty();
    return true;
}

float App::preview_probe_mirror_center_x() const {
    const PreviewProbe* first_enabled_probe = nullptr;
    for (const auto& probe : preview_probes) {
        if (probe.enabled) {
            first_enabled_probe = &probe;
            break;
        }
    }

    if (!first_enabled_probe) {
        return 0.5f;
    }

    float min_x = first_enabled_probe->fixture.position[0];
    float max_x = first_enabled_probe->fixture.position[0];
    for (const auto& probe : preview_probes) {
        if (!probe.enabled) {
            continue;
        }
        min_x = std::min(min_x, probe.fixture.position[0]);
        max_x = std::max(max_x, probe.fixture.position[0]);
    }
    return (min_x + max_x) * 0.5f;
}

PreviewOutputSample App::preview_sample_from_world(Vec3 position) const {
    if (preview_samples.empty()) {
        return {};
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

PreviewProbe* App::find_preview_probe_by_name(std::string_view name) {
    for (auto& probe : preview_probes) {
        if (probe.fixture.name == name) {
            return &probe;
        }
    }
    return nullptr;
}

const PreviewProbe* App::find_preview_probe_by_name(std::string_view name) const {
    for (const auto& probe : preview_probes) {
        if (probe.fixture.name == name) {
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

bool App::sync_mirrored_preview_probe(const PreviewProbe& source_probe, bool copy_traits) {
    const std::optional<std::string> mirrored_name = mirrored_probe_name(source_probe.fixture.name);
    if (!mirrored_name.has_value()) {
        return false;
    }

    PreviewProbe* mirrored_probe = find_preview_probe_by_name(*mirrored_name);
    if (!mirrored_probe || mirrored_probe->fixture.id == source_probe.fixture.id) {
        return false;
    }

    const float center_x = preview_probe_mirror_center_x();
    const Vec3 next_position{
        center_x + (center_x - source_probe.fixture.position[0]),
        source_probe.fixture.position[1],
        source_probe.fixture.position[2],
    };

    bool changed = false;
    const Vec3 previous_position = mirrored_probe->fixture.position;
    const float dx = next_position[0] - previous_position[0];
    const float dy = next_position[1] - previous_position[1];
    const float dz = next_position[2] - previous_position[2];
    if ((dx * dx + dy * dy + dz * dz) > 0.00000025f) {
        mirrored_probe->fixture.position = next_position;
        changed = true;
    }

    if (copy_traits && mirrored_probe->fixture.traits != source_probe.fixture.traits) {
        mirrored_probe->fixture.traits = source_probe.fixture.traits;
        changed = true;
    }

    return changed;
}

std::string App::current_preview_output_label() const {
    const Node* preview_node = graph.find_node(preview_node_id);
    if (!preview_node || preview_output_socket_index < 0 ||
        preview_output_socket_index >= (int)preview_node->outputs.size()) {
        return "Sample";
    }
    return preview_node->outputs[(std::size_t)preview_output_socket_index].name;
}

std::optional<ValueType> App::current_preview_output_type() const {
    const Node* preview_node = graph.find_node(preview_node_id);
    if (!preview_node || preview_output_socket_index < 0 ||
        preview_output_socket_index >= (int)preview_node->outputs.size()) {
        return std::nullopt;
    }
    return preview_node->outputs[(std::size_t)preview_output_socket_index].type;
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

PreviewOutputSample App::extract_preview_output(const Graph& source_graph) const {
    const auto preview_type = current_preview_output_type();
    if (!preview_type.has_value()) {
        return {};
    }

    switch (*preview_type) {
        case ValueType::Scalar:
            return PreviewOutputSample{
                extract_node_output_scalar(source_graph, preview_node_id,
                                           (std::size_t)preview_output_socket_index),
                std::nullopt,
            };
        case ValueType::Vec3:
            return PreviewOutputSample{
                std::nullopt,
                extract_node_output_vec3(source_graph, preview_node_id,
                                         (std::size_t)preview_output_socket_index),
            };
        default:
            return {};
    }
}

std::optional<float> App::extract_node_output_scalar(const Graph& source_graph, NodeId node_id,
                                                     std::size_t output_index) const {
    if (node_id == 0) {
        return std::nullopt;
    }

    const Node* source_node = source_graph.find_node(node_id);
    if (!source_node || output_index >= source_node->outputs.size()) {
        return std::nullopt;
    }

    if (source_node->outputs[output_index].type != ValueType::Scalar) {
        return std::nullopt;
    }

    return std::get<Scalar>(source_node->outputs[output_index].current);
}

std::optional<Vec3> App::extract_node_output_vec3(const Graph& source_graph, NodeId node_id,
                                                  std::size_t output_index) const {
    if (node_id == 0) {
        return std::nullopt;
    }

    const Node* source_node = source_graph.find_node(node_id);
    if (!source_node || output_index >= source_node->outputs.size()) {
        return std::nullopt;
    }

    if (source_node->outputs[output_index].type != ValueType::Vec3) {
        return std::nullopt;
    }

    return std::get<Vec3>(source_node->outputs[output_index].current);
}

bool App::rebuild_preview_graphs() {
    preview_graphs.clear();
    preview_samples.assign((std::size_t)(preview_grid_width * preview_grid_height), {});
    preview_graphs.reserve((std::size_t)(preview_grid_width * preview_grid_height));

    const Vec3 previous_probe_position = current_builtin_probe_position();
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

    const Vec3 previous_probe_position = current_builtin_probe_position();
    GraphBuildError err;
    for (const auto& probe : preview_probes) {
        if (!probe.enabled) {
            continue;
        }

        PreviewProbeRuntime runtime;
        runtime.probe_id = probe.fixture.id;
        runtime.graph = graph;
        runtime.graph.initialized = false;
        set_builtin_probe_position(probe.fixture.position);
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
        preview_samples[i] = extract_preview_output(preview_graphs[i]);
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

        const PreviewOutputSample exact_sample = extract_preview_output(runtime.graph);
        const std::optional<float> dimmer_value = fixture_has_trait(probe->fixture, FixtureTrait::Dimmer)
            ? extract_node_output_scalar(runtime.graph, spatial_fixture_driver_node_id, 0)
            : std::nullopt;
        const std::optional<float> tilt_value = fixture_has_trait(probe->fixture, FixtureTrait::Tilt)
            ? extract_node_output_scalar(runtime.graph, spatial_fixture_driver_node_id, 1)
            : std::nullopt;
        const std::optional<Vec3> color_value = fixture_has_trait(probe->fixture, FixtureTrait::ColorRGB)
            ? extract_node_output_vec3(runtime.graph, spatial_fixture_driver_node_id, 2)
            : std::nullopt;
        preview_probe_samples.push_back(PreviewProbeSample{
            probe->fixture.id,
            probe->fixture.position,
            exact_sample.scalar_value
                ? std::optional<float>{std::clamp(*exact_sample.scalar_value, 0.0f, 1.0f)}
                : std::nullopt,
            exact_sample.color_value
                ? std::optional<Vec3>{Vec3{
                    std::clamp((*exact_sample.color_value)[0], 0.0f, 1.0f),
                    std::clamp((*exact_sample.color_value)[1], 0.0f, 1.0f),
                    std::clamp((*exact_sample.color_value)[2], 0.0f, 1.0f),
                }}
                : std::nullopt,
            dimmer_value ? std::optional<float>{std::clamp(*dimmer_value, 0.0f, 1.0f)} : std::nullopt,
            tilt_value ? std::optional<float>{std::clamp(*tilt_value, 0.0f, 1.0f)} : std::nullopt,
            color_value ? std::optional<Vec3>{Vec3{
                std::clamp((*color_value)[0], 0.0f, 1.0f),
                std::clamp((*color_value)[1], 0.0f, 1.0f),
                std::clamp((*color_value)[2], 0.0f, 1.0f),
            }} : std::nullopt,
        });
    }
}

void App::draw_node(Node& n) {
    if (!ImGui::TreeNode(&n, "Node #%llu  '%s'  [%s]%s",
                         (unsigned long long)n.id,
                         n.name.c_str(),
                         n.type->name.c_str(),
                         n.bypass ? " [bypassed]" : "")) {
        return;
    }

    ImGui::Text("Position: (%.1f, %.1f)", n.position[0], n.position[1]);
    ImGui::Checkbox("Bypass", &n.bypass);
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete Node")) {
        pending_delete_node_id = n.id;
    }

    char name_buffer[128];
    std::snprintf(name_buffer, sizeof(name_buffer), "%s", n.name.c_str());
    if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
        n.name = name_buffer;
    }

    std::vector<char> comments_buffer(std::max<std::size_t>(n.comments.size() + 256, 1024), '\0');
    std::snprintf(comments_buffer.data(), comments_buffer.size(), "%s", n.comments.c_str());
    if (ImGui::InputTextMultiline("Comments", comments_buffer.data(), comments_buffer.size(),
                                  ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f))) {
        n.comments = comments_buffer.data();
    }

    if (n.type && n.type->name == "BPMTap") {
        const auto tap_history = bpm_tap_history_seconds.find(n.id);
        const std::size_t tap_count = tap_history != bpm_tap_history_seconds.end()
            ? tap_history->second.size()
            : 0;
        const Scalar bpm = std::get<Scalar>(n.state.at("bpm"));
        const float period = bpm > 0.0f ? 60.0f / bpm : 0.0f;

        ImGui::Separator();
        ImGui::Text("Tap Tempo");
        ImGui::Text("Tempo: %.1f BPM", bpm);
        ImGui::SameLine();
        ImGui::TextDisabled("Beat Period: %.3fs", period);
        if (tap_count < 2) {
            ImGui::TextDisabled("Tap twice to derive tempo from real intervals.");
        } else {
            ImGui::TextDisabled("Averaging %zu recent tap intervals.", tap_count - 1);
        }
        if (ImGui::Button("Tap Tempo")) {
            tap_bpm(n);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Taps")) {
            clear_bpm_tap_history(n.id);
        }
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
        const Socket* src_output = (src_node && connection.source.socket_index < src_node->outputs.size())
            ? &src_node->outputs[connection.source.socket_index]
            : nullptr;
        const Socket* dst_input = (dst_node && connection.destination.socket_index < dst_node->inputs.size())
            ? &dst_node->inputs[connection.destination.socket_index]
            : nullptr;
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
        if (src_output && dst_input) {
            ImGui::TextDisabled("    %s  source=%s  input=%s",
                                value_type_name(src_output->type),
                                format_value(src_output->current).c_str(),
                                format_value(dst_input->current).c_str());
        }
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
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        if (!graph.nodes[i].outputs.empty()) {
            source_nodes.push_back((int)i);
        }
    }

    if (source_nodes.empty()) {
        ImGui::TextUnformatted("Need at least one source node with outputs.");
        return;
    }

    if (source_node_selection >= (int)source_nodes.size()) {
        source_node_selection = 0;
    }

    Node& source_node = graph.nodes[(std::size_t)source_nodes[(std::size_t)source_node_selection]];

    if (source_output_selection >= (int)source_node.outputs.size()) {
        source_output_selection = 0;
    }

    if (ImGui::BeginCombo("Source Node", source_node.name.c_str())) {
        for (std::size_t i = 0; i < source_nodes.size(); ++i) {
            const bool selected = ((int)i == source_node_selection);
            const auto& node = graph.nodes[(std::size_t)source_nodes[i]];
            if (ImGui::Selectable(node.name.c_str(), selected)) {
                source_node_selection = (int)i;
                source_output_selection = 0;
                destination_node_selection = 0;
                destination_input_selection = 0;
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
                destination_node_selection = 0;
                destination_input_selection = 0;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const Socket& source_output = source_node.outputs[(std::size_t)source_output_selection];
    ImGui::Text("Connection Type: %s", value_type_name(source_output.type));
    ImGui::Text("Source Value: %s", format_value(source_output.current).c_str());

    auto input_already_connected = [&](NodeId node_id, std::size_t input_index) {
        for (const auto& connection : graph.connections) {
            if (connection.destination.node_id == node_id &&
                connection.destination.socket_index == input_index) {
                return true;
            }
        }
        return false;
    };

    auto compatible_inputs_for_node = [&](const Node& node) {
        std::vector<int> compatible_inputs;
        compatible_inputs.reserve(node.inputs.size());
        for (std::size_t i = 0; i < node.inputs.size(); ++i) {
            if (node.inputs[i].type == source_output.type &&
                !input_already_connected(node.id, i)) {
                compatible_inputs.push_back((int)i);
            }
        }
        return compatible_inputs;
    };

    std::vector<int> compatible_destination_nodes;
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        if (graph.nodes[i].id == source_node.id) {
            continue;
        }
        if (!compatible_inputs_for_node(graph.nodes[i]).empty()) {
            compatible_destination_nodes.push_back((int)i);
        }
    }

    if (compatible_destination_nodes.empty()) {
        ImGui::TextUnformatted("No destination inputs accept the selected output type.");
        return;
    }

    if (destination_node_selection >= (int)compatible_destination_nodes.size()) {
        destination_node_selection = 0;
    }

    Node& destination_node = graph.nodes[(std::size_t)compatible_destination_nodes[(std::size_t)destination_node_selection]];
    const std::vector<int> compatible_inputs = compatible_inputs_for_node(destination_node);

    if (compatible_inputs.empty()) {
        ImGui::TextUnformatted("Selected destination node has no compatible inputs.");
        return;
    }

    if (destination_input_selection >= (int)compatible_inputs.size()) {
        destination_input_selection = 0;
    }

    if (ImGui::BeginCombo("Destination Node", destination_node.name.c_str())) {
        for (std::size_t i = 0; i < compatible_destination_nodes.size(); ++i) {
            const bool selected = ((int)i == destination_node_selection);
            const auto& node = graph.nodes[(std::size_t)compatible_destination_nodes[i]];
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

    const int selected_destination_input = compatible_inputs[(std::size_t)destination_input_selection];
    if (ImGui::BeginCombo("Destination Input", destination_node.inputs[(std::size_t)selected_destination_input].name.c_str())) {
        for (std::size_t i = 0; i < compatible_inputs.size(); ++i) {
            const bool selected = ((int)i == destination_input_selection);
            const int input_index = compatible_inputs[i];
            if (ImGui::Selectable(destination_node.inputs[(std::size_t)input_index].name.c_str(), selected)) {
                destination_input_selection = (int)i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const Socket& destination_input = destination_node.inputs[(std::size_t)selected_destination_input];
    ImGui::Text("Destination Current: %s", format_value(destination_input.current).c_str());
    ImGui::Text("Destination Default: %s", format_value(destination_input.default_value).c_str());

    if (ImGui::Button("Add Connection")) {
        try_add_connection(source_node.id, (std::size_t)source_output_selection,
                           destination_node.id, (std::size_t)selected_destination_input);
    }
}

void App::draw_field_preview_panel() {
    if (!ImGui::CollapsingHeader("Field Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ensure_preview_probe_selection();

    auto preview_sample_intensity = [](const PreviewOutputSample& sample) -> float {
        if (sample.scalar_value.has_value()) {
            return std::clamp(*sample.scalar_value, 0.0f, 1.0f);
        }
        if (sample.color_value.has_value()) {
            const Vec3 color = *sample.color_value;
            return std::clamp((color[0] + color[1] + color[2]) / 3.0f, 0.0f, 1.0f);
        }
        return 0.0f;
    };

    auto preview_sample_color = [&](const PreviewOutputSample& sample) -> ImVec4 {
        if (sample.color_value.has_value()) {
            const Vec3 color = *sample.color_value;
            return ImVec4(color[0], color[1], color[2], 1.0f);
        }
        const float value = preview_sample_intensity(sample);
        return ImVec4(value, value * 0.8f, 0.15f + value * 0.85f, 1.0f);
    };

    std::vector<const Node*> preview_output_nodes;
    for (const auto& node : graph.nodes) {
        for (const auto& output : node.outputs) {
            if (output.type == ValueType::Scalar || output.type == ValueType::Vec3) {
                preview_output_nodes.push_back(&node);
                break;
            }
        }
    }

    if (preview_output_nodes.empty()) {
        ImGui::TextUnformatted("Need at least one node with a scalar or Vec3 output.");
        return;
    }

    if (!graph.find_node(preview_node_id)) {
        preview_node_id = preview_output_nodes.front()->id;
        preview_output_socket_index = 0;
    }

    const Node* preview_node = graph.find_node(preview_node_id);
    if (!preview_node) {
        ImGui::TextUnformatted("Preview target is unavailable.");
        return;
    }

    if (ImGui::BeginCombo("Preview Node", preview_node->name.c_str())) {
        for (const Node* node : preview_output_nodes) {
            const bool selected = node->id == preview_node_id;
            if (ImGui::Selectable(node->name.c_str(), selected)) {
                preview_node_id = node->id;
                preview_output_socket_index = 0;
                preview_node = node;
                refresh_preview_samples();
                refresh_preview_probe_samples();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    std::vector<std::size_t> preview_outputs;
    for (std::size_t i = 0; i < preview_node->outputs.size(); ++i) {
        if (preview_node->outputs[i].type == ValueType::Scalar ||
            preview_node->outputs[i].type == ValueType::Vec3) {
            preview_outputs.push_back(i);
        }
    }

    if (preview_outputs.empty()) {
        ImGui::TextUnformatted("Selected node has no scalar or Vec3 outputs.");
        return;
    }

    if (std::find(preview_outputs.begin(), preview_outputs.end(),
                  (std::size_t)preview_output_socket_index) == preview_outputs.end()) {
        preview_output_socket_index = (int)preview_outputs.front();
    }

    if (ImGui::BeginCombo("Preview Output", preview_node->outputs[(std::size_t)preview_output_socket_index].name.c_str())) {
        for (const std::size_t output_index : preview_outputs) {
            const bool selected = ((int)output_index == preview_output_socket_index);
            if (ImGui::Selectable(preview_node->outputs[output_index].name.c_str(), selected)) {
                preview_output_socket_index = (int)output_index;
                refresh_preview_samples();
                refresh_preview_probe_samples();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (const auto preview_type = current_preview_output_type()) {
        ImGui::Text("Preview Type: %s", value_type_name(*preview_type));
    }

    auto normalize_bounds = [](float& min_value, float& max_value) {
        if (min_value > max_value) {
            std::swap(min_value, max_value);
        }
        if ((max_value - min_value) < 0.001f) {
            max_value = min_value + 0.001f;
        }
    };

    if (ImGui::BeginCombo("Preview Plane", preview_plane_name(preview_plane))) {
        for (const PreviewPlane candidate : {PreviewPlane::XY, PreviewPlane::XZ, PreviewPlane::YZ}) {
            const bool selected = candidate == preview_plane;
            if (ImGui::Selectable(preview_plane_name(candidate), selected)) {
                preview_plane = candidate;
                preview_slice = world_axis_value(live_probe_position(), preview_plane_slice_axis(preview_plane));
                fit_preview_domain_to_probes();
                mark_preview_dirty();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const auto preview_axes = preview_plane_axes(preview_plane);
    const int preview_slice_axis = preview_plane_slice_axis(preview_plane);

    char bounds_label[32];
    char slice_label[32];
    std::snprintf(bounds_label, sizeof(bounds_label), "%s Bounds", world_axis_name(preview_axes[0]));
    float u_bounds[2] = { preview_u_min, preview_u_max };
    if (ImGui::DragFloat2(bounds_label, u_bounds, 0.01f)) {
        preview_u_min = u_bounds[0];
        preview_u_max = u_bounds[1];
        normalize_bounds(preview_u_min, preview_u_max);
        mark_preview_dirty();
    }

    std::snprintf(bounds_label, sizeof(bounds_label), "%s Bounds", world_axis_name(preview_axes[1]));
    float v_bounds[2] = { preview_v_min, preview_v_max };
    if (ImGui::DragFloat2(bounds_label, v_bounds, 0.01f)) {
        preview_v_min = v_bounds[0];
        preview_v_max = v_bounds[1];
        normalize_bounds(preview_v_min, preview_v_max);
        mark_preview_dirty();
    }

    std::snprintf(slice_label, sizeof(slice_label), "Preview %s", world_axis_name(preview_slice_axis));
    if (ImGui::DragFloat(slice_label, &preview_slice, 0.01f)) {
        mark_preview_dirty();
    }

    if (ImGui::Button("Reset Domain")) {
        preview_u_min = 0.0f;
        preview_u_max = 1.0f;
        preview_v_min = 0.0f;
        preview_v_max = 1.0f;
        preview_slice = 0.0f;
        mark_preview_dirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit Domain To Probes")) {
        fit_preview_domain_to_probes();
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
    const ImVec2 preview_size{
        ((preview_grid_width - 1) * (cell_size + cell_padding)) + cell_size,
        ((preview_grid_height - 1) * (cell_size + cell_padding)) + cell_size,
    };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    struct ScreenProbeInfo {
        const PreviewProbe* probe = nullptr;
        PreviewOutputSample sample;
        float intensity = 0.0f;
        ImVec2 center;
    };
    std::vector<ScreenProbeInfo> screen_probes;
    screen_probes.reserve(preview_probes.size());

    for (int y = 0; y < preview_grid_height; ++y) {
        for (int x = 0; x < preview_grid_width; ++x) {
            const std::size_t index = (std::size_t)(y * preview_grid_width + x);

            const ImVec2 p1{
                origin.x + x * (cell_size + cell_padding),
                origin.y + y * (cell_size + cell_padding),
            };
            const ImVec2 p2{ p1.x + cell_size, p1.y + cell_size };
            const ImU32 color = ImGui::GetColorU32(preview_sample_color(preview_samples[index]));

            draw_list->AddRectFilled(p1, p2, color, 4.0f);
            draw_list->AddRect(p1, p2, ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 1.0f)), 4.0f);
        }
    }

    if (show_preview_probes) {
        const float overlay_slice_epsilon = std::max(std::max(preview_u_max - preview_u_min, preview_v_max - preview_v_min) * 0.08f, 0.05f);
        for (const auto& probe : preview_probes) {
            if (!probe.enabled) {
                continue;
            }
            const float slice_distance = std::fabs(world_axis_value(probe.fixture.position, preview_slice_axis) - preview_slice);
            if (slice_distance > overlay_slice_epsilon) {
                continue;
            }
            const Vec2 normalized = preview_normalized_from_world(probe.fixture.position);
            const PreviewProbeSample* exact_sample = find_preview_probe_sample(probe.fixture.id);
            const PreviewOutputSample preview_sample = exact_sample
                ? PreviewOutputSample{ exact_sample->preview_scalar_value, exact_sample->preview_color_value }
                : PreviewOutputSample{};
            const float sample = preview_sample_intensity(preview_sample);
            const ImVec2 center{
                origin.x + normalized[0] * ((preview_grid_width - 1) * (cell_size + cell_padding)),
                origin.y + normalized[1] * ((preview_grid_height - 1) * (cell_size + cell_padding)),
            };
            screen_probes.push_back(ScreenProbeInfo{
                &probe,
                preview_sample,
                sample,
                center,
            });
        }

        auto probe_group_key = [](const std::string& name) {
            std::string key = name;
            while (!key.empty() && std::isdigit((unsigned char)key.back())) {
                key.pop_back();
            }
            while (!key.empty() && std::isspace((unsigned char)key.back())) {
                key.pop_back();
            }
            return key;
        };

        std::unordered_map<std::string, std::vector<std::size_t>> guide_groups;
        for (std::size_t i = 0; i < screen_probes.size(); ++i) {
            const std::string key = probe_group_key(screen_probes[i].probe->fixture.name);
            if (!key.empty()) {
                guide_groups[key].push_back(i);
            }
        }

        for (auto& [_, indices] : guide_groups) {
            if (indices.size() < 2) {
                continue;
            }

            std::sort(indices.begin(), indices.end(),
                      [&](std::size_t a, std::size_t b) {
                          return world_axis_value(screen_probes[a].probe->fixture.position, preview_axes[1])
                              < world_axis_value(screen_probes[b].probe->fixture.position, preview_axes[1]);
                      });

            std::vector<ImVec2> path;
            path.reserve(indices.size());
            for (const std::size_t index : indices) {
                path.push_back(screen_probes[index].center);
            }
            draw_list->AddPolyline(path.data(), (int)path.size(),
                                   ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f)),
                                   0, 2.0f);
        }

        for (const auto& screen_probe : screen_probes) {
            const auto& probe = *screen_probe.probe;
            const bool selected = selected_preview_probe_id.has_value() && *selected_preview_probe_id == probe.fixture.id;
            const ImU32 ring_color = ImGui::GetColorU32(
                selected ? ImVec4(1.0f, 0.95f, 0.35f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
            const ImVec4 probe_color = preview_sample_color(screen_probe.sample);
            const ImU32 fill_color = ImGui::GetColorU32(probe_color);
            draw_list->AddCircleFilled(screen_probe.center, 5.0f, fill_color);
            draw_list->AddCircle(screen_probe.center, 7.0f, ring_color, 0, 2.0f);

            char label[32];
            if (current_preview_output_type() == ValueType::Scalar) {
                std::snprintf(label, sizeof(label), "%s %.2f", probe.fixture.name.c_str(), screen_probe.intensity);
            } else {
                std::snprintf(label, sizeof(label), "%s", probe.fixture.name.c_str());
            }
            draw_list->AddText(ImVec2(screen_probe.center.x + 8.0f, screen_probe.center.y - 8.0f),
                               ring_color,
                               label);
        }
    }

    ImGui::Dummy(preview_size);
    const ImGuiIO& io = ImGui::GetIO();
    if (show_preview_probes &&
        ImGui::IsWindowHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = io.MousePos;
        if (mouse.x >= origin.x && mouse.x <= (origin.x + preview_size.x) &&
            mouse.y >= origin.y && mouse.y <= (origin.y + preview_size.y)) {
            const ScreenProbeInfo* nearest_probe = nullptr;
            float nearest_distance_sq = 14.0f * 14.0f;
            for (const auto& screen_probe : screen_probes) {
                const float dx = mouse.x - screen_probe.center.x;
                const float dy = mouse.y - screen_probe.center.y;
                const float distance_sq = dx * dx + dy * dy;
                if (distance_sq <= nearest_distance_sq) {
                    nearest_distance_sq = distance_sq;
                    nearest_probe = &screen_probe;
                }
            }
            if (nearest_probe) {
                selected_preview_probe_id = nearest_probe->probe->fixture.id;
                dragged_preview_probe_id = nearest_probe->probe->fixture.id;
            }
        }
    }
    if (dragged_preview_probe_id.has_value()) {
        if (!io.MouseDown[ImGuiMouseButton_Left]) {
            dragged_preview_probe_id.reset();
        } else if (PreviewProbe* dragged_probe = find_preview_probe(*dragged_preview_probe_id)) {
            const float preview_width = std::max(preview_size.x, 1.0f);
            const float preview_height = std::max(preview_size.y, 1.0f);
            const float normalized_x = std::clamp((io.MousePos.x - origin.x) / preview_width, 0.0f, 1.0f);
            const float normalized_y = std::clamp((io.MousePos.y - origin.y) / preview_height, 0.0f, 1.0f);
            const Vec3 next_position = preview_world_position_from_normalized(
                Vec2{normalized_x, normalized_y},
                world_axis_value(dragged_probe->fixture.position, preview_slice_axis));
            const Vec3 previous_position = dragged_probe->fixture.position;
            const float du = world_axis_value(next_position, preview_axes[0]) - world_axis_value(previous_position, preview_axes[0]);
            const float dv = world_axis_value(next_position, preview_axes[1]) - world_axis_value(previous_position, preview_axes[1]);
            if ((du * du + dv * dv) > 0.00000025f) {
                dragged_probe->fixture.position = next_position;
                if (lock_mirrored_preview_pairs) {
                    sync_mirrored_preview_probe(*dragged_probe, false);
                }
                mark_preview_dirty();
            }
        } else {
            dragged_preview_probe_id.reset();
        }
    }
    const Vec3 center = preview_probe_center();
    const std::optional<ValueType> preview_type = current_preview_output_type();
    float preview_min = 1.0f;
    float preview_max = 0.0f;
    float preview_sum = 0.0f;
    std::size_t preview_count = 0;
    Vec3 average_color{0.0f, 0.0f, 0.0f};
    std::size_t color_count = 0;
    for (const auto& sample : preview_samples) {
        const float intensity = preview_sample_intensity(sample);
        preview_min = std::min(preview_min, intensity);
        preview_max = std::max(preview_max, intensity);
        preview_sum += intensity;
        ++preview_count;
        if (sample.color_value.has_value()) {
            average_color[0] += (*sample.color_value)[0];
            average_color[1] += (*sample.color_value)[1];
            average_color[2] += (*sample.color_value)[2];
            ++color_count;
        }
    }
    if (preview_count == 0) {
        preview_min = 0.0f;
        preview_max = 0.0f;
    }
    if (color_count > 0) {
        average_color[0] /= (float)color_count;
        average_color[1] /= (float)color_count;
        average_color[2] /= (float)color_count;
    }
    ImGui::Text("Plane: %s  Domain: %s %.2f..%.2f, %s %.2f..%.2f, %s %.2f",
                preview_plane_name(preview_plane),
                world_axis_name(preview_axes[0]), preview_u_min, preview_u_max,
                world_axis_name(preview_axes[1]), preview_v_min, preview_v_max,
                world_axis_name(preview_slice_axis), preview_slice);
    ImGui::Text("Preview range: min %.3f  max %.3f  avg %.3f",
                preview_min, preview_max,
                preview_count > 0 ? (preview_sum / (float)preview_count) : 0.0f);
    if (preview_type == ValueType::Vec3) {
        ImGui::ColorButton("Average Preview Color",
                           ImVec4(average_color[0], average_color[1], average_color[2], 1.0f),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(24.0f, 24.0f));
        ImGui::SameLine();
        ImGui::Text("Average Color: [%.3f, %.3f, %.3f]",
                    average_color[0], average_color[1], average_color[2]);
    }
    if (const PreviewProbe* selected_probe = selected_preview_probe()) {
        ImGui::Text("Selected probe: %s at [%.2f, %.2f, %.2f]",
                    selected_probe->fixture.name.c_str(),
                    selected_probe->fixture.position[0],
                    selected_probe->fixture.position[1],
                    selected_probe->fixture.position[2]);
        if (const PreviewProbeSample* selected_sample = find_preview_probe_sample(selected_probe->fixture.id)) {
            if (selected_sample->preview_scalar_value.has_value()) {
                ImGui::Text("Selected preview sample: %.3f", *selected_sample->preview_scalar_value);
            } else if (selected_sample->preview_color_value.has_value()) {
                const Vec3 color = *selected_sample->preview_color_value;
                ImGui::ColorButton("Selected Preview Color",
                                   ImVec4(color[0], color[1], color[2], 1.0f),
                                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                                   ImVec2(24.0f, 24.0f));
                ImGui::SameLine();
                ImGui::Text("Selected preview color: [%.3f, %.3f, %.3f]",
                            color[0], color[1], color[2]);
            }
        }
    } else {
        ImGui::Text("Inspector probe center: [%.2f, %.2f, %.2f]", center[0], center[1], center[2]);
    }
    if (show_preview_probes) {
        std::size_t enabled_count = 0;
        for (const auto& probe : preview_probes) {
            if (probe.enabled) {
                ++enabled_count;
            }
        }
        ImGui::Text("Probe overlay: %zu/%zu enabled sample points intersect the current %s slice.",
                    screen_probes.size(),
                    enabled_count,
                    preview_plane_name(preview_plane));
    }
    ImGui::Text("Prototype preview: one persistent graph state per sampled probe.");

    if (ImGui::TreeNode("Preview Probes")) {
        if (ImGui::Button("Add Probe")) {
            const std::size_t next_index = preview_probes.size() + 1;
            preview_probes.push_back(PreviewProbe{
                FixtureProbe{
                    next_preview_probe_id++,
                    "Probe " + std::to_string(next_index),
                    preview_world_position_from_normalized(Vec2{0.5f, 0.5f}, preview_slice),
                    { FixtureTrait::Dimmer, FixtureTrait::Tilt, FixtureTrait::ColorRGB },
                },
                true,
            });
            selected_preview_probe_id = preview_probes.back().fixture.id;
            mark_preview_dirty();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Lock mirrored pairs", &lock_mirrored_preview_pairs);

        std::optional<uint64_t> probe_to_delete;
        for (auto& probe : preview_probes) {
            ImGui::PushID((int)probe.fixture.id);
            const std::optional<std::string> mirrored_name = mirrored_probe_name(probe.fixture.name);
            const bool has_mirrored_pair = mirrored_name.has_value() &&
                find_preview_probe_by_name(*mirrored_name) != nullptr;

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
            if (has_mirrored_pair) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Sync Pair")) {
                    if (sync_mirrored_preview_probe(probe, true)) {
                        mark_preview_dirty();
                    }
                }
            }

            float world_xyz[3] = {
                probe.fixture.position[0],
                probe.fixture.position[1],
                probe.fixture.position[2],
            };
            if (ImGui::DragFloat3("World XYZ", world_xyz, 0.01f)) {
                probe.fixture.position = Vec3{world_xyz[0], world_xyz[1], world_xyz[2]};
                if (lock_mirrored_preview_pairs) {
                    sync_mirrored_preview_probe(probe, false);
                }
                mark_preview_dirty();
            }

            bool dimmer = fixture_has_trait(probe.fixture, FixtureTrait::Dimmer);
            bool traits_changed = false;
            if (ImGui::Checkbox("Dimmer Trait", &dimmer)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Dimmer, dimmer);
                traits_changed = true;
            }
            ImGui::SameLine();
            bool pan = fixture_has_trait(probe.fixture, FixtureTrait::Pan);
            if (ImGui::Checkbox("Pan Trait", &pan)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Pan, pan);
                traits_changed = true;
            }
            ImGui::SameLine();
            bool tilt = fixture_has_trait(probe.fixture, FixtureTrait::Tilt);
            if (ImGui::Checkbox("Tilt Trait", &tilt)) {
                fixture_set_trait(probe.fixture, FixtureTrait::Tilt, tilt);
                traits_changed = true;
            }
            ImGui::SameLine();
            bool color_rgb = fixture_has_trait(probe.fixture, FixtureTrait::ColorRGB);
            if (ImGui::Checkbox("ColorRGB Trait", &color_rgb)) {
                fixture_set_trait(probe.fixture, FixtureTrait::ColorRGB, color_rgb);
                traits_changed = true;
            }
            if (traits_changed && lock_mirrored_preview_pairs) {
                if (sync_mirrored_preview_probe(probe, true)) {
                    mark_preview_dirty();
                }
            }

            const PreviewProbeSample* exact_sample = find_preview_probe_sample(probe.fixture.id);
            const PreviewOutputSample sample = exact_sample
                ? PreviewOutputSample{ exact_sample->preview_scalar_value, exact_sample->preview_color_value }
                : preview_sample_from_world(probe.fixture.position);
            ImGui::Text("Traits: %s", format_fixture_traits(probe.fixture.traits).c_str());
            ImGui::Text("World XYZ: [%.2f, %.2f, %.2f]",
                        probe.fixture.position[0],
                        probe.fixture.position[1],
                        probe.fixture.position[2]);
            if (sample.scalar_value.has_value()) {
                ImGui::Text("Preview sample: %.3f", *sample.scalar_value);
            } else if (sample.color_value.has_value()) {
                const Vec3 color = *sample.color_value;
                ImGui::ColorButton("Preview Color",
                                   ImVec4(color[0], color[1], color[2], 1.0f),
                                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                                   ImVec2(24.0f, 24.0f));
                ImGui::SameLine();
                ImGui::Text("Preview Color: [%.3f, %.3f, %.3f]",
                            color[0], color[1], color[2]);
            } else {
                ImGui::TextUnformatted("Preview sample: unavailable");
            }
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
                if (sample.preview_scalar_value.has_value()) {
                    ImGui::ProgressBar(*sample.preview_scalar_value, ImVec2(-1.0f, 0.0f), sample_label.c_str());
                    ImGui::Text("%s sample: %.3f", sample_label.c_str(), *sample.preview_scalar_value);
                } else if (sample.preview_color_value.has_value()) {
                    const Vec3 color = *sample.preview_color_value;
                    ImGui::ColorButton("Preview Output",
                                       ImVec4(color[0], color[1], color[2], 1.0f),
                                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                    ImGui::Text("%s sample: [%.3f, %.3f, %.3f]",
                                sample_label.c_str(),
                                color[0], color[1], color[2]);
                } else {
                    ImGui::Text("%s sample: unavailable", sample_label.c_str());
                }
                if (sample.dimmer_value.has_value() || sample.tilt_value.has_value()) {
                    char dimmer_buffer[16] = "--";
                    char tilt_buffer[16] = "--";
                    if (sample.dimmer_value.has_value()) {
                        std::snprintf(dimmer_buffer, sizeof(dimmer_buffer), "%.3f", *sample.dimmer_value);
                    }
                    if (sample.tilt_value.has_value()) {
                        std::snprintf(tilt_buffer, sizeof(tilt_buffer), "%.3f", *sample.tilt_value);
                    }
                    ImGui::Text("Fixture driver: dimmer %s  tilt %s",
                                dimmer_buffer,
                                tilt_buffer);
                }
                if (sample.driver_color_value.has_value()) {
                    const Vec3 color = *sample.driver_color_value;
                    ImGui::ColorButton("ColorRGB",
                                       ImVec4(color[0], color[1], color[2], 1.0f),
                                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                    ImGui::Text("Color RGB: [%.3f, %.3f, %.3f]",
                                color[0], color[1], color[2]);
                }
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
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        io.KeyCtrl &&
        ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        focus_node_spawn_filter = true;
    }
    ImGui::SetNextItemWidth(240.0f);
    if (focus_node_spawn_filter) {
        ImGui::SetKeyboardFocusHere();
        focus_node_spawn_filter = false;
    }
    ImGui::InputTextWithHint("##NodeSpawnFilter", "Search node types", node_spawn_filter,
                             sizeof(node_spawn_filter));
    const bool spawn_filter_active = ImGui::IsItemActive();

    auto lowercase_copy = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return text;
    };

    std::vector<const NodeType*> filtered_spawn_types;
    const std::string filter = lowercase_copy(node_spawn_filter);
    filtered_spawn_types.reserve(all.size());
    for (const auto& [name, type] : all) {
        const std::string searchable = lowercase_copy(
            name + " " + type.display_name + " " + type.category);
        if (filter.empty() || searchable.find(filter) != std::string::npos) {
            filtered_spawn_types.push_back(&type);
        }
    }

    if (!filtered_spawn_types.empty()) {
        if (filtered_spawn_selection >= (int)filtered_spawn_types.size()) {
            filtered_spawn_selection = 0;
        }
        if (spawn_filter_active) {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
                filtered_spawn_selection = (filtered_spawn_selection + 1)
                    % (int)filtered_spawn_types.size();
            } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
                filtered_spawn_selection = (filtered_spawn_selection - 1
                    + (int)filtered_spawn_types.size()) % (int)filtered_spawn_types.size();
            }
        }
        const NodeType* selected_spawn_type =
            filtered_spawn_types[(std::size_t)filtered_spawn_selection];
        if (ImGui::BeginCombo("Quick Add Type", selected_spawn_type->display_name.c_str())) {
            for (std::size_t i = 0; i < filtered_spawn_types.size(); ++i) {
                const bool selected = ((int)i == filtered_spawn_selection);
                const NodeType* type = filtered_spawn_types[i];
                const std::string label = type->display_name + " [" + type->category + "]";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    filtered_spawn_selection = (int)i;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Add Filtered Type")) {
            spawn_node(selected_spawn_type->name.c_str());
        }
        if (spawn_filter_active && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
            spawn_node(selected_spawn_type->name.c_str());
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu match%s",
                            filtered_spawn_types.size(),
                            filtered_spawn_types.size() == 1 ? "" : "es");
    } else {
        filtered_spawn_selection = 0;
        ImGui::TextDisabled("No matching node types.");
    }

    if (ImGui::Button("Reset Diagonal Sweep")) {
        reset_default_patch();
    }
    ImGui::SameLine();
    if (ImGui::Button("Constant"))      spawn_node("Constant");
    ImGui::SameLine();
    if (ImGui::Button("Constant Vec3")) spawn_node("ConstantVec3");
    ImGui::SameLine();
    if (ImGui::Button("BPM Tap"))       spawn_node("BPMTap");
    ImGui::SameLine();
    if (ImGui::Button("Phase"))         spawn_node("Phase");
    ImGui::SameLine();
    if (ImGui::Button("Ramp"))          spawn_node("Ramp");
    ImGui::SameLine();
    if (ImGui::Button("Add"))           spawn_node("Add");
    ImGui::SameLine();
    if (ImGui::Button("Multiply"))      spawn_node("Multiply");
    ImGui::SameLine();
    if (ImGui::Button("Mix"))           spawn_node("Mix");
    ImGui::SameLine();
    if (ImGui::Button("Mix Vec3"))      spawn_node("MixVec3");
    ImGui::SameLine();
    if (ImGui::Button("Clamp"))         spawn_node("Clamp");
    ImGui::SameLine();
    if (ImGui::Button("Output Dimmer")) spawn_node("OutputDimmer");
    ImGui::SameLine();
    if (ImGui::Button("Output Tilt"))   spawn_node("OutputTilt");
    ImGui::SameLine();
    if (ImGui::Button("Fixture Driver")) spawn_node("SpatialFixtureDriver");
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
    if (ImGui::Button("ProbeZ"))        spawn_node("ProbeZ");
    ImGui::SameLine();
    if (ImGui::Button("Project 2D"))    spawn_node("Project2D");
    ImGui::SameLine();
    if (ImGui::Button("Project 3D"))    spawn_node("Project3D");
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
