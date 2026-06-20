#include "ComputeNode.h"
#include <iostream>
#include <imgui.h>

namespace lit
{

    ComputeNode::ComputeNode(const std::string &name, int gridSize)
        : Node(name), m_GridSize(gridSize), m_NumFixtures(gridSize * gridSize)
    {
        m_Fixtures.resize(m_NumFixtures);
    }

    ComputeNode::~ComputeNode()
    {
        if (m_ShaderProgram)
            glDeleteProgram(m_ShaderProgram);
        if (m_SSBO)
        {
            glDeleteBuffers(1, &m_SSBO);
        }
    }

    GLuint ComputeNode::CompileShader(const char *source)
    {
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "[ComputeNode] Shader Error in " << m_Name << ":\n"
                      << infoLog << std::endl;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        glDeleteShader(shader);
        return program;
    }

    void ComputeNode::Initialize()
    {
        const char *computeSource = R"(
        #version 460 core
        layout(local_size_x = 256) in;

        struct Fixture {
            float x, y;
            float channels[6];
        };

        layout(std430, binding = 0) buffer FixtureBuffer {
            Fixture fixtures[];
        };

        uniform float u_Time;
        uniform uint u_NumFixtures;

        void main() {
            uint i = gl_GlobalInvocationID.x;
            if(i >= u_NumFixtures) return;

            vec2 pos = vec2(fixtures[i].x, fixtures[i].y);

            // Generator Math
            float wave = (sin(pos.x * 10.0 - u_Time * 4.0) + 1.0) * 0.5;
            float pulse = (cos(pos.y * 10.0 + u_Time * 3.0) + 1.0) * 0.5;

            // Map to generic channels
            fixtures[i].channels[0] = 1.0;          // Dimmer
            fixtures[i].channels[1] = wave;         // R
            fixtures[i].channels[2] = pulse * 0.5;  // G
            fixtures[i].channels[3] = 1.0 - wave;   // B
            fixtures[i].channels[4] = wave;         // Pan (Example)
            fixtures[i].channels[5] = pulse;        // Tilt (Example)
        }
    )";

        m_ShaderProgram = CompileShader(computeSource);
        m_TimeLoc = glGetUniformLocation(m_ShaderProgram, "u_Time");
        m_NumFixLoc = glGetUniformLocation(m_ShaderProgram, "u_NumFixtures");

        // Map logical coordinates
        for (int y = 0; y < m_GridSize; y++)
        {
            for (int x = 0; x < m_GridSize; x++)
            {
                m_Fixtures[y * m_GridSize + x].x = (float)x / (m_GridSize - 1);
                m_Fixtures[y * m_GridSize + x].y = (float)y / (m_GridSize - 1);
            }
        }

        glGenBuffers(1, &m_SSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_Fixtures.size() * sizeof(FixtureData), m_Fixtures.data(), GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_SSBO);
    }

    void ComputeNode::Process(float deltaTime, float globalTime)
    {
        glUseProgram(m_ShaderProgram);
        glUniform1f(m_TimeLoc, globalTime);
        glUniform1ui(m_NumFixLoc, m_NumFixtures);

        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        FixtureData *mappedData = (FixtureData *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, m_Fixtures.size() * sizeof(FixtureData), GL_MAP_READ_BIT);

        if (mappedData)
        {
            memcpy(m_Fixtures.data(), mappedData, m_Fixtures.size() * sizeof(FixtureData));
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    void ComputeNode::DrawUI()
    {
        ImGui::PushID(m_Name.c_str());
        ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Name.c_str());

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Compute Generator Node");
        ImGui::Separator();
        ImGui::Text("Fixtures: %d", m_NumFixtures);
        ImGui::Text("SSBO ID: %d", m_SSBO);

        if (ImGui::TreeNode("Raw Channel Output (Fixture 0)"))
        {
            ImGui::Text("Dimmer: %.2f", m_Fixtures[0].channels[0]);
            ImGui::Text("R: %.2f G: %.2f B: %.2f", m_Fixtures[0].channels[1], m_Fixtures[0].channels[2], m_Fixtures[0].channels[3]);
            ImGui::Text("Pan: %.2f Tilt: %.2f", m_Fixtures[0].channels[4], m_Fixtures[0].channels[5]);
            ImGui::TreePop();
        }

        ImGui::End();
        ImGui::PopID();
    }

} // namespace lit