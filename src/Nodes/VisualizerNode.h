#pragma once
#include "../Node.h"
#include "ComputeNode.h"

namespace lit
{

    class VisualizerNode : public Node
    {
    private:
        ComputeNode &m_InputNode; // It references the output of a ComputeNode
        int m_GridSize;

    public:
        VisualizerNode(const std::string &name, ComputeNode &input, int gridSize = 16);
        void Initialize() override {} // No setup needed for visualizer
        void Process(float deltaTime, float globalTime) override {}
        void DrawUI() override;
    };

} // namespace lit