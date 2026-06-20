#include <iostream>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include "Engine.h"

namespace lit
{

    Engine::Engine() {}

    Engine::~Engine()
    {
        Shutdown();
    }

    bool Engine::Initialize()
    {
        if (!InitializeSDL())
            return false;
        if (!InitializeImGui())
            return false;

        m_StartTicks = SDL_GetTicks();
        m_Compute = std::make_unique<lit::ComputeNode>("Compute Generator");
        m_Compute->Initialize();
        m_Visualizer = std::make_unique<lit::VisualizerNode>("Stage View", *m_Compute);
        return true;
    }

    bool Engine::InitializeSDL()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            std::cerr << "[Engine] SDL Init Failed: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

        m_Window = SDL_CreateWindow("lit Engine - Node Architecture", m_Width, m_Height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!m_Window)
            return false;

        m_GLContext = SDL_GL_CreateContext(m_Window);
        if (!m_GLContext)
            return false;

        SDL_GL_MakeCurrent(m_Window, m_GLContext);
        SDL_GL_SetSwapInterval(1); // VSync

        if (gl3wInit())
        {
            std::cerr << "[Engine] GL3W Init Failed" << std::endl;
            return false;
        }

        std::cout << "\n=== lit Engine Core ===" << std::endl;
        std::cout << "Vendor:   " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "=======================\n"
                  << std::endl;

        return true;
    }

    bool Engine::InitializeImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForOpenGL(m_Window, m_GLContext);
        ImGui_ImplOpenGL3_Init("#version 460");
        return true;
    }

    float Engine::GetTime() const
    {
        return (SDL_GetTicks() - m_StartTicks) / 1000.0f;
    }

    void Engine::HandleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                m_Running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window))
            {
                m_Running = false;
            }
        }
    }

    void Engine::RenderUI()
    {
        // Top Level Engine Diagnostics
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 80), ImGuiCond_Always);
        ImGui::Begin("Engine Core", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "SYSTEM ONLINE");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }

    void Engine::Run()
    {
        m_Running = true;

        while (m_Running)
        {
            HandleEvents();

            SDL_GetWindowSizeInPixels(m_Window, &m_Width, &m_Height);
            glViewport(0, 0, m_Width, m_Height);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // ImGui Frame Prep
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // NOTE: Here is where we will loop through and execute the Nodes later
            // nodeManager.UpdateAll();

            m_Compute->Process(0.0f, GetTime());
            m_Visualizer->DrawUI();
            m_Compute->DrawUI();

            RenderUI();

            // ImGui Render Execution
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(m_Window);
        }
    }

    void Engine::Shutdown()
    {
        if (m_GLContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            SDL_GL_DestroyContext(m_GLContext);
            m_GLContext = nullptr;
        }
        if (m_Window)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }
        SDL_Quit();
    }

} // namespace lit