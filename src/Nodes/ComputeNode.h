#pragma once
#include "../Node.h"
#include "../LitCore.h"
#include <GL/gl3w.h>
#include <vector>

namespace lit
{

    class ComputeNode : public Node
    {
    private:
        GLuint m_ShaderProgram = 0;
        GLuint m_SSBO = 0;
        GLint m_TimeLoc = 0;
        GLint m_NumFixLoc = 0;

        int m_GridSize;
        int m_NumFixtures;
        std::vector<FixtureData> m_Fixtures;

        GLuint CompileShader(const char *source);

    public:
        ComputeNode(const std::string &name, int gridSize = 16);
        ~ComputeNode() override;

        void Initialize() override;
        void Process(float deltaTime, float globalTime) override;
        void DrawUI() override;

        const std::vector<FixtureData> &GetData() const { return m_Fixtures; }
    };

} // namespace lit