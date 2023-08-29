#pragma once

#include <cinttypes>

namespace NTSCify
{
  struct ScreenSettings
  {
    // How much to barrel distort our picture horizontally and vertically to emulate a curved screen
    float horizontalDistortion = 0.0f;
    float verticalDistortion = 0.0f;

    // How much additional rounding of the edges we want to emulate a screen that didn't have a rectangular bezel shape
    float screenEdgeRoundingX = 0.0f;
    float screenEdgeRoundingY = 0.0f;

    // How much to round the corners (to emulate an old TV with rounded corners)
    float cornerRounding = 0.0f;

    // How much the shadow 
    float shadowMaskScale = 1.0f;
    float shadowMaskStrength = 0.0f;

    // How much of the previous frame to keep around on the next frame.
    float phosphorDecay = 0.0f;

    // How powerful the scanlines are
    float scanlineStrength = 0.0f;

    // How much the "glass" in front of the "phosphors" diffuses the light passing through it.
    float diffusionStrength = 0.0f;
  };


  struct ScreenSettingsPreset
  {
    const char *name;
    ScreenSettings settings;
  };


  static constexpr ScreenSettingsPreset k_screenPresets[] =
  {
    {
      "Nothing At All",
      {
        0.0f,
        0.0f,
        
        0.0f,
        0.0f,
        
        0.0f,
        
        1.0f,
        0.0f,
        
        0.0f,

        0.0f,

        0.0f,
      }
    },
    {
      "Scanlines Only",
      {
        0.0f,
        0.0f,
        
        0.0f,
        0.0f,
        
        0.0f,
        
        1.0f,
        0.0f,
        
        0.25f,

        0.47f,

        0.0f,
      }
    },
    {
      "Flat CRT",
      {
        0.0f,
        0.0f,
        
        0.0f,
        0.0f,
        
        0.0f,
        
        1.0f,
        0.65f,
        
        0.25f,

        0.47f,

        0.5f,
      }
    },
    {
      "Standard CRT",
      {
        0.35f,
        0.25f,
        
        0.0f,
        0.0f,
        
        0.03f,
        
        1.0f,
        0.65f,
        
        0.25f,

        0.47f,

        0.5f,
      }
    },
    {
      "Standard CRT (No Scanlines)",
      {
        0.35f,
        0.25f,
        
        0.0f,
        0.0f,
        
        0.03f,
        
        1.0f,
        0.65f,
        
        0.25f,

        0.00f,

        0.5f,
      }
    },
    {
      "Old CRT",
      {
        0.30f,
        0.35f,
        
        0.1f,
        0.1f,
        
        0.1f,
        
        1.2f,
        0.65f,
        
        0.25f,

        0.47f,

        0.7f,
      }
    },
  };
}