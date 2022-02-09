#pragma once

#include <cinttypes>

namespace NTSCify::CRT
{
  struct ScreenSettings
  {
    float inputPixelAspectRatio = 1.0f; // 1/1 for square input pixels, but for the NES it's 8/7

    uint32_t overscanLeft = 0;
    uint32_t overscanRight = 0;
    uint32_t overscanTop = 0;
    uint32_t overscanBottom = 0;

    float horizontalDistortion = 0;
    float verticalDistortion = 0;

    float screenEdgeRoundingX = 0.0f;
    float screenEdgeRoundingY = 0.0f;

    float cornerRounding = 0;

    float shadowMaskScale = 1.1f;
    float shadowMaskStrength = 0.85f;

    float phosphorDecay = 0.05f;

    float scanlineStrength = 0.25f;

    float instabilityScale = 0.0f;
  };
}