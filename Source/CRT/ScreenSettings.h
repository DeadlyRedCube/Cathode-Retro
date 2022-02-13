#pragma once

#include <cinttypes>

namespace NTSCify::CRT
{
  struct ScreenSettings
  {
    // The display aspect ratio of an input pixel when displayed.
    //  Use 1/1 for square input pixels, but for the NES/SNES it's 8/7
    float inputPixelAspectRatio = 1.0f;     

    // Each of these represents how many input pixels of overscan there is on each side of the view
    uint32_t overscanLeft = 0;
    uint32_t overscanRight = 0;
    uint32_t overscanTop = 0;
    uint32_t overscanBottom = 0;

    // How much to barrel distort our picture horizontally and vertically to emulate a curved screen
    float horizontalDistortion = 0.20f;
    float verticalDistortion = 0.25f;

    // How much additional rounding of the edges we want to emulate a screen that didn't have a rectangular bezel shape
    float screenEdgeRoundingX = 0.0f;
    float screenEdgeRoundingY = 0.0f;

    // How much to round the corners (to emulate an old TV with rounded corners)
    float cornerRounding = 0.05f;

    // How much the shadow 
    float shadowMaskScale = 1.0f;
    float shadowMaskStrength = 1.0f;

    // How much of the previous frame to keep around on the next frame.
    float phosphorDecay = 0.05f;

    // How powerful the scanlines are
    float scanlineStrength = 0.25f;
  };
}