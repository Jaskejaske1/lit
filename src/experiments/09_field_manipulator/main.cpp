// experiment_09 — Raylib Field Manipulator
//
// A stable 3D viewport for controlling a scalar field over a festival-style rig.
// CPU-side field computation for now; GPU re-integration can be added later.
//
// Coordinate system:
//   X = left / right (0..1)
//   Y = up / down    (0..1)
//   Z = front / back (0..1)
//
// Controls:
//   Z / S          = move source forward / backward
//   Q / D          = move source left / right
//   A / E          = move source down / up
//   T / G          = bigger / smaller radius
//   U / J          = more / less intensity
//   1 / 2          = radial / vertical sweep field
//   Space          = toggle automatic motion
//   Left drag      = orbit
//   Middle drag    = pan
//   Scroll         = zoom
//   Esc            = quit

#include "raylib.h"
#include <cmath>
#include <vector>
#include <algorithm>


#if defined(_WIN32)
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#pragma comment(linker, "/include:NvOptimusEnablement")
#pragma comment(linker, "/include:AmdPowerXpressRequestHighPerformance")
#endif

constexpr int kFieldX = 10;
constexpr int kFieldY = 8;
constexpr int kFieldZ = 5;
constexpr int kFieldCount = kFieldX * kFieldY * kFieldZ;
constexpr int kNumFixtures = 24;

// Fire-like colour ramp.
static Color heatColor(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);

    if (t < 0.15f)
    {
        float x = t / 0.15f;
        return Color{(unsigned char)(0), (unsigned char)(0), (unsigned char)(20 + 60 * x), 255};
    }
    else if (t < 0.45f)
    {
        float x = (t - 0.15f) / 0.30f;
        return Color{(unsigned char)(242 * x), (unsigned char)(51 * x), (unsigned char)(84 - 71 * x), 255};
    }
    else if (t < 0.75f)
    {
        float x = (t - 0.45f) / 0.30f;
        return Color{242, (unsigned char)(51 + 166 * x), (unsigned char)(13 - 13 * x), 255};
    }
    else
    {
        float x = (t - 0.75f) / 0.25f;
        return Color{255, (unsigned char)(217 - 217 * x), 0, 255};
    }
}

// Scalar field evaluation.
static float fieldValue(Vector3 p, float time, int mode,
                        Vector3 source, float radius, float intensity)
{
    if (mode == 1)
    {
        // Radial energy source.
        float dx = p.x - source.x;
        float dy = p.y - source.y;
        float dz = p.z - source.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        return std::clamp(intensity * std::exp(-d2 / (radius * radius)), 0.0f, 1.0f);
    }
    else
    {
        // Vertical sweep plane: energy depends on X and Y.
        float dx = p.x - source.x;
        float dy = p.y - source.y;
        float d2 = dx * dx + 0.2f * dy * dy;
        return std::clamp(intensity * std::exp(-d2 / (radius * radius)), 0.0f, 1.0f);
    }
}

int main()
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Experiment 09 - Field Manipulator");
    SetTargetFPS(60);

    // Camera: starts at front, looking at stage centre.
    Camera3D camera = {0};
    camera.position = Vector3{1.6f, 1.4f, -1.6f};
    camera.target = Vector3{0.5f, 0.3f, 0.5f};
    camera.fovy = 40.0f;
    camera.up = Vector3{0.0f, 1.0f, 0.0f};

    camera.projection = CAMERA_PERSPECTIVE;

    float camDx = camera.position.x - camera.target.x;
    float camDy = camera.position.y - camera.target.y;
    float camDz = camera.position.z - camera.target.z;
    float camDistance = std::sqrt(camDx * camDx + camDy * camDy + camDz * camDz);
    float camYaw = std::atan2(camDx, camDz);
    float camPitch = std::asin(camDy / camDistance);

    // Fixture rig.
    std::vector<Vector3> fixtures;
    fixtures.reserve(kNumFixtures);

    // 8 back truss at Z = 1.
    for (int i = 0; i < 8; ++i)
    {
        float x = 0.12f + 0.76f * ((float)i / 7.0f);
        float y = 0.85f + 0.10f * std::sin((x - 0.5f) * PI);
        fixtures.push_back(Vector3{x, y, 1.0f});
    }

    // 8 front floor at Z = 0.
    for (int i = 0; i < 8; ++i)
    {
        float x = 0.10f + 0.80f * ((float)i / 7.0f);
        fixtures.push_back(Vector3{x, 0.05f, 0.0f});
    }

    // 8 side booms.
    const float sideY[4] = {0.15f, 0.40f, 0.65f, 0.90f};
    for (int side = 0; side < 2; ++side)
    {
        float x = side == 0 ? 0.03f : 0.97f;
        for (int row = 0; row < 4; ++row)
        {
            fixtures.push_back(Vector3{x, sideY[row], 0.40f});
        }
    }

    // Field point cloud.
    std::vector<Vector3> fieldPoints;
    fieldPoints.reserve(kFieldCount);
    for (int zi = 0; zi < kFieldZ; ++zi)
    {
        float z = (float)zi / (float)(kFieldZ - 1);
        for (int yi = 0; yi < kFieldY; ++yi)
        {
            float y = (float)yi / (float)(kFieldY - 1);
            for (int xi = 0; xi < kFieldX; ++xi)
            {
                float x = (float)xi / (float)(kFieldX - 1);
                fieldPoints.push_back(Vector3{x, y, z});
            }
        }
    }

    // Field state.
    Vector3 source = Vector3{0.5f, 0.5f, 0.5f};
    float radius = 0.35f;
    float intensity = 1.2f;
    int fieldMode = 1;
    bool autoMotion = false;
    float time = 0.0f;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        time += dt;

        // Camera controls: drag orbit, middle drag pan, scroll zoom.
        Vector2 mouseDelta = GetMouseDelta();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            camYaw -= mouseDelta.x * 0.008f;
            camPitch += mouseDelta.y * 0.008f;
            camPitch = std::clamp(camPitch, -1.45f, 1.45f);
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            float panScale = 0.0025f * camDistance;

            float fx = camera.target.x - camera.position.x;
            float fz = camera.target.z - camera.position.z;
            float fl = std::sqrt(fx * fx + fz * fz);
            if (fl > 0.0001f)
            {
                fx /= fl;
                fz /= fl;
            }
            else
            {
                fx = 0.0f;
                fz = 1.0f;
            }

            float rightX = fz;
            float rightZ = -fx;

            camera.target.x -= mouseDelta.x * panScale * rightX;
            camera.target.z -= mouseDelta.x * panScale * rightZ;
            camera.target.y += mouseDelta.y * panScale;
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            camDistance *= (1.0f - wheel * 0.10f);
            camDistance = std::clamp(camDistance, 0.5f, 12.0f);
        }

        float cp = std::cos(camPitch);
        camera.position.x = camera.target.x + camDistance * cp * std::sin(camYaw);
        camera.position.y = camera.target.y + camDistance * std::sin(camPitch);
        camera.position.z = camera.target.z + camDistance * cp * std::cos(camYaw);

        bool cameraManipulating = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                                  IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

        // Field movement (camera-relative on ground plane).
        bool moved = false;
        const float moveSpeed = 0.8f * dt;

        Vector3 forward = Vector3{
            camera.target.x - camera.position.x,
            0.0f,
            camera.target.z - camera.position.z};
        float forwardLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        if (forwardLen > 0.0001f)
        {
            forward.x /= forwardLen;
            forward.z /= forwardLen;
        }
        else
        {
            forward = Vector3{0.0f, 0.0f, 1.0f};
        }

        // right = cross(worldUp, forward)
        Vector3 right = Vector3{-forward.z, 0.0f, forward.x};

        if (!cameraManipulating && IsKeyDown(KEY_W))
        {
            source.x += forward.x * moveSpeed;
            source.z += forward.z * moveSpeed;
            moved = true;
        }
        if (!cameraManipulating && IsKeyDown(KEY_S))
        {
            source.x -= forward.x * moveSpeed;
            source.z -= forward.z * moveSpeed;
            moved = true;
        }
        if (!cameraManipulating && IsKeyDown(KEY_A))
        {
            source.x -= right.x * moveSpeed;
            source.z -= right.z * moveSpeed;
            moved = true;
        }
        if (!cameraManipulating && IsKeyDown(KEY_D))
        {
            source.x += right.x * moveSpeed;
            source.z += right.z * moveSpeed;
            moved = true;
        }
        if (!cameraManipulating && IsKeyDown(KEY_Q))
        {
            source.y -= moveSpeed;
            moved = true;
        }
        if (!cameraManipulating && IsKeyDown(KEY_E))
        {
            source.y += moveSpeed;
            moved = true;
        }

        if (IsKeyPressed(KEY_T))
            radius = std::min(1.0f, radius + 0.03f);
        if (IsKeyPressed(KEY_G))
            radius = std::max(0.03f, radius - 0.03f);
        if (IsKeyPressed(KEY_U))
            intensity = std::min(1.5f, intensity + 0.1f);
        if (IsKeyPressed(KEY_J))
            intensity = std::max(0.0f, intensity - 0.1f);
        if (IsKeyPressed(KEY_ONE))
            fieldMode = 1;
        if (IsKeyPressed(KEY_TWO))
            fieldMode = 2;
        if (IsKeyPressed(KEY_SPACE))
            autoMotion = !autoMotion;

        if (moved)
            autoMotion = false;

        if (autoMotion)
        {
            source.x = 0.5f + 0.24f * std::sin(time * 0.8f);
            source.y = 0.5f + 0.20f * std::sin(time * 0.5f);
            source.z = 0.5f + 0.30f * std::sin(time * 0.6f);
        }

        source.x = std::clamp(source.x, 0.0f, 1.0f);
        source.y = std::clamp(source.y, 0.0f, 1.0f);
        source.z = std::clamp(source.z, 0.0f, 1.0f);

        BeginDrawing();
        ClearBackground(Color{28, 28, 32, 255});

        BeginMode3D(camera);

        // Custom floor grid (dark grey).
        for (int i = 0; i <= 10; ++i)
        {
            float pos = (float)i / 10.0f;
            DrawLine3D(Vector3{pos, 0.0f, 0.0f}, Vector3{pos, 0.0f, 1.0f}, Color{55, 55, 60, 255});
            DrawLine3D(Vector3{0.0f, 0.0f, pos}, Vector3{1.0f, 0.0f, pos}, Color{55, 55, 60, 255});
        }

        // Coordinate axes.
        DrawLine3D(Vector3{0, 0, 0}, Vector3{1.2f, 0, 0}, RED);
        DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 1.2f, 0}, GREEN);
        DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 0, 1.2f}, BLUE);

        // Field points.
        for (const Vector3 &p : fieldPoints)
        {
            float v = fieldValue(p, time, fieldMode, source, radius, intensity);
            if (v < 0.03f)
                continue;

            // Cube voxels are significantly cheaper than spheres on weaker GPUs.
            DrawCubeV(p, Vector3{0.02f, 0.02f, 0.02f}, heatColor(v));
        }

        // Fixtures.
        for (const Vector3 &f : fixtures)
        {
            float v = fieldValue(f, time, fieldMode, source, radius, intensity);
            Color c = heatColor(v);
            DrawCube(f, 0.04f, 0.04f, 0.04f, c);
            DrawCubeWires(f, 0.04f, 0.04f, 0.04f, WHITE);
        }

        EndMode3D();

        // HUD.
        DrawFPS(10, 10);
        DrawText(TextFormat("Mode %d | Source: %.2f %.2f %.2f | Radius: %.2f | Intensity: %.2f",
                            fieldMode, source.x, source.y, source.z, radius, intensity),
                 10, 30, 16, LIGHTGRAY);
        DrawText("Z/Q/S/D move, A/E height, T/G radius, U/J intensity, 1/2 mode, Space auto",
                 10, 50, 16, LIGHTGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}