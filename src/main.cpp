#include "Engine.h"

// FORCE HIGH-PERFORMANCE GPU
#ifdef _WIN32
#include <windows.h>
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
    lit::Engine engine;

    if (!engine.Initialize())
    {
        return -1;
    }

    // Transfers control to the main loop inside Engine.cpp
    engine.Run();

    return 0;
}