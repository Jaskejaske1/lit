#pragma once

namespace lit
{

    // 32-byte aligned structure (perfect for std430 SSBO layout)
    struct FixtureData
    {
        float x, y;        // 8 bytes (Logical position 0.0 -> 1.0)
        float channels[6]; // 24 bytes (e.g., [0]=Dimmer, [1]=R, [2]=G, [3]=B, [4]=Pan, [5]=Tilt)
    };

} // namespace lit