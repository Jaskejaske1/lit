#pragma once
#include <memory>
#include <SDL3/SDL.h>
#include <GL/gl3w.h>
#include "Nodes/ComputeNode.h"
#include "Nodes/VisualizerNode.h"

namespace lit
{

    class Engine
    {
    private:
        SDL_Window *m_Window = nullptr;
        SDL_GLContext m_GLContext = nullptr;
        bool m_Running = false;
        int m_Width = 1280;
        int m_Height = 720;
        Uint64 m_StartTicks = 0;

        bool InitializeSDL();
        bool InitializeImGui();
        void HandleEvents();
        void RenderUI();
        
        std::unique_ptr<lit::ComputeNode> m_Compute;
        std::unique_ptr<lit::VisualizerNode> m_Visualizer;

    public:
        Engine();
        ~Engine();

        bool Initialize();
        void Run();
        void Shutdown();

        // Utility accessors
        float GetTime() const;
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
    };

} // namespace lit