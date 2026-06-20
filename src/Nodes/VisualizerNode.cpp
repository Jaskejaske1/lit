#include "VisualizerNode.h"
#include <imgui.h>

namespace lit
{

    VisualizerNode::VisualizerNode(const std::string &name, ComputeNode &input, int gridSize)
        : Node(name), m_InputNode(input), m_GridSize(gridSize) {}

    void VisualizerNode::DrawUI()
    {
        ImGui::PushID(m_Name.c_str());
        ImGui::SetNextWindowPos(ImVec2(350, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Name.c_str());

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        float rectSize = 30.0f;
        float padding = 5.0f;

        const auto &fixtures = m_InputNode.GetData();

        for (int i = 0; i < fixtures.size(); i++)
        {
            int gridX = i % m_GridSize;

            int gridY = i / m_GridSize;

            ImVec2 pos1 = ImVec2(origin.x + gridX * (rectSize + padding), origin.y + gridY * (rectSize + padding));
            ImVec2 pos2 = ImVec2(pos1.x + rectSize, pos1.y + rectSize);

            // Map generic channels [1], [2], [3] (R, G, B) to the UI
            ImU32 color = ImGui::GetColorU32(ImVec4(fixtures[i].channels[1], fixtures[i].channels[2], fixtures[i].channels[3], 1.0f));
            drawList->AddRectFilled(pos1, pos2, color, 4.0f);
        }

        ImGui::End();
        ImGui::PopID();
    }

} // namespace lit