#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "node.h"

// lit — substrate graph core
//
// This is the first real runtime container under the substrate:
//   - Connection is a separate graph object, not embedded in sockets.
//   - Graph owns instantiated nodes + connections.
//   - bake() validates wiring and computes a stable topological order once.
//   - init_pass() and tick() drive node evaluation in that order.
//
// Data model references:
//   - Connection: docs/data-model.txt "Connection"
//   - Graph: docs/data-model.txt "Graph"
//   - Topological evaluation order: docs/data-model.txt "Topological evaluation order"
//   - Initialization: docs/data-model.txt "Initialization (cold start, warm start, init pass)"

namespace substrate {

using ConnectionId = uint64_t;

struct SocketEndpoint {
    NodeId      node_id = 0;
    std::size_t socket_index = 0;
};

struct Connection {
    ConnectionId   id = 0;
    SocketEndpoint source;
    SocketEndpoint destination;
};

enum class GraphBakeState : uint8_t { Editing, Baked };

enum class GraphBuildErrorCode : uint8_t {
    None,
    DuplicateNodeId,
    MissingSourceNode,
    MissingDestinationNode,
    SourceSocketOutOfRange,
    DestinationSocketOutOfRange,
    SourceSocketNotOutput,
    DestinationSocketNotInput,
    TypeMismatch,
    CycleDetected,
};

struct GraphBuildError {
    GraphBuildErrorCode code = GraphBuildErrorCode::None;
    ConnectionId        connection_id = 0;
    NodeId              node_id = 0;
    SocketEndpoint      endpoint{};
};

inline std::string_view graph_build_error_name(GraphBuildErrorCode code) {
    switch (code) {
        case GraphBuildErrorCode::None: return "None";
        case GraphBuildErrorCode::DuplicateNodeId: return "DuplicateNodeId";
        case GraphBuildErrorCode::MissingSourceNode: return "MissingSourceNode";
        case GraphBuildErrorCode::MissingDestinationNode: return "MissingDestinationNode";
        case GraphBuildErrorCode::SourceSocketOutOfRange: return "SourceSocketOutOfRange";
        case GraphBuildErrorCode::DestinationSocketOutOfRange: return "DestinationSocketOutOfRange";
        case GraphBuildErrorCode::SourceSocketNotOutput: return "SourceSocketNotOutput";
        case GraphBuildErrorCode::DestinationSocketNotInput: return "DestinationSocketNotInput";
        case GraphBuildErrorCode::TypeMismatch: return "TypeMismatch";
        case GraphBuildErrorCode::CycleDetected: return "CycleDetected";
    }
    return "UnknownGraphBuildError";
}

struct Graph {
    std::vector<Node>                     nodes;
    std::vector<Connection>               connections;
    GraphBakeState                        bake_state = GraphBakeState::Editing;
    std::vector<std::size_t>              evaluation_order;
    std::vector<std::vector<std::size_t>> incoming_connection_indices;
    float                                 elapsed_seconds = 0.0f;
    bool                                  initialized = false;

    void mark_dirty() {
        bake_state = GraphBakeState::Editing;
        evaluation_order.clear();
        incoming_connection_indices.clear();
        initialized = false;
    }

    Node* find_node(NodeId id) {
        for (auto& node : nodes) {
            if (node.id == id) {
                return &node;
            }
        }
        return nullptr;
    }

    const Node* find_node(NodeId id) const {
        for (const auto& node : nodes) {
            if (node.id == id) {
                return &node;
            }
        }
        return nullptr;
    }

    GraphBuildError bake() {
        GraphBuildError ok;
        mark_dirty();

        std::unordered_map<NodeId, std::size_t> node_index_by_id;
        node_index_by_id.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto [_, inserted] = node_index_by_id.emplace(nodes[i].id, i);
            if (!inserted) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::DuplicateNodeId,
                    .node_id = nodes[i].id,
                };
            }
        }

        incoming_connection_indices.assign(nodes.size(), {});
        std::vector<std::vector<std::size_t>> outgoing_connection_indices(nodes.size());
        std::vector<std::size_t> indegree(nodes.size(), 0);

        for (std::size_t i = 0; i < connections.size(); ++i) {
            const auto& connection = connections[i];

            auto src_it = node_index_by_id.find(connection.source.node_id);
            if (src_it == node_index_by_id.end()) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::MissingSourceNode,
                    .connection_id = connection.id,
                    .endpoint = connection.source,
                };
            }

            auto dst_it = node_index_by_id.find(connection.destination.node_id);
            if (dst_it == node_index_by_id.end()) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::MissingDestinationNode,
                    .connection_id = connection.id,
                    .endpoint = connection.destination,
                };
            }

            const auto src_index = src_it->second;
            const auto dst_index = dst_it->second;
            const auto& src_node = nodes[src_index];
            const auto& dst_node = nodes[dst_index];

            if (connection.source.socket_index >= src_node.outputs.size()) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::SourceSocketOutOfRange,
                    .connection_id = connection.id,
                    .endpoint = connection.source,
                };
            }

            if (connection.destination.socket_index >= dst_node.inputs.size()) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::DestinationSocketOutOfRange,
                    .connection_id = connection.id,
                    .endpoint = connection.destination,
                };
            }

            const auto& src_socket = src_node.outputs[connection.source.socket_index];
            const auto& dst_socket = dst_node.inputs[connection.destination.socket_index];

            if (src_socket.direction != SocketDirection::Output) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::SourceSocketNotOutput,
                    .connection_id = connection.id,
                    .endpoint = connection.source,
                };
            }

            if (dst_socket.direction != SocketDirection::Input) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::DestinationSocketNotInput,
                    .connection_id = connection.id,
                    .endpoint = connection.destination,
                };
            }

            if (src_socket.type != dst_socket.type) {
                return GraphBuildError{
                    .code = GraphBuildErrorCode::TypeMismatch,
                    .connection_id = connection.id,
                    .endpoint = connection.destination,
                };
            }

            incoming_connection_indices[dst_index].push_back(i);
            outgoing_connection_indices[src_index].push_back(i);
            ++indegree[dst_index];
        }

        std::vector<std::size_t> ready;
        ready.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (indegree[i] == 0) {
                ready.push_back(i);
            }
        }

        for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
            const auto node_index = ready[cursor];
            evaluation_order.push_back(node_index);

            for (const auto connection_index : outgoing_connection_indices[node_index]) {
                const auto& connection = connections[connection_index];
                const auto dst_index = node_index_by_id.at(connection.destination.node_id);
                if (--indegree[dst_index] == 0) {
                    ready.push_back(dst_index);
                }
            }
        }

        if (evaluation_order.size() != nodes.size()) {
            return GraphBuildError{ .code = GraphBuildErrorCode::CycleDetected };
        }

        bake_state = GraphBakeState::Baked;
        return ok;
    }

    bool init_pass(GraphBuildError* error = nullptr) {
        if (bake_state != GraphBakeState::Baked) {
            GraphBuildError bake_error = bake();
            if (bake_error.code != GraphBuildErrorCode::None) {
                if (error) {
                    *error = bake_error;
                }
                return false;
            }
        }

        for (const auto node_index : evaluation_order) {
            prime_inputs_(node_index);
            evaluate_node_(nodes[node_index], 0.0f, elapsed_seconds, true);
        }

        initialized = true;
        if (error) {
            *error = GraphBuildError{};
        }
        return true;
    }

    bool tick(float dt, GraphBuildError* error = nullptr) {
        if (!initialized && !init_pass(error)) {
            return false;
        }

        elapsed_seconds += dt;
        for (const auto node_index : evaluation_order) {
            prime_inputs_(node_index);
            evaluate_node_(nodes[node_index], dt, elapsed_seconds, false);
        }

        if (error) {
            *error = GraphBuildError{};
        }
        return true;
    }

  private:
    void prime_inputs_(std::size_t node_index) {
        auto& node = nodes[node_index];

        for (auto& input : node.inputs) {
            input.current = input.default_value;
        }

        for (const auto connection_index : incoming_connection_indices[node_index]) {
            const auto& connection = connections[connection_index];
            const Node* src_node = find_node(connection.source.node_id);
            if (!src_node) {
                continue;
            }

            node.inputs[connection.destination.socket_index].current =
                src_node->outputs[connection.source.socket_index].current;
        }
    }

    static void evaluate_node_(Node& node, float dt, float elapsed, bool init_pass) {
        if (node.bypass) {
            for (auto& output : node.outputs) {
                output.current = output.default_value;
            }
            return;
        }

        if (node.type && node.type->evaluate) {
            node.type->evaluate(node, dt, elapsed, init_pass);
        }
    }
};

}  // namespace substrate
