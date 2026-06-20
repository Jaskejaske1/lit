#pragma once
#include <string>

namespace lit
{

    class Node
    {
    protected:
        std::string m_Name;

    public:
        Node(const std::string &name) : m_Name(name) {}
        virtual ~Node() = default;

        // The execution pipeline
        virtual void Initialize() = 0;
        virtual void Process(float deltaTime, float globalTime) = 0;
        virtual void DrawUI() = 0;

        const std::string &GetName() const { return m_Name; }
    };

} // namespace lit