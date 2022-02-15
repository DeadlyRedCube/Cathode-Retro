#pragma once

namespace NTSCify
{
  struct ArtifactSettings
  {
    float ghostVisibility = 0.0f;           // How visible is the ghost
    float ghostSpreadScale = 0.71f;         // How far does each sample of the ghost spread from the last
    float ghostDistance    = 3.1f;          // How far from center is the ghost's center

    float noiseStrength = 0.0f;             // How much noise is added to the signal

    float instabilityScale = 0.0f;          // How much horizontal wobble to have on the screen per scanline

    float temporalArtifactReduction = 0.0f; // How much to blend between two different phases to reduce temporal aliasing
  };

  struct ArtifactSettingsPreset
  {
    const char *name;
    ArtifactSettings settings;
  };

  static constexpr ArtifactSettingsPreset k_artifactPresets[] =
  {
    {
      "Pristine",
      {
        0.0f, 
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
      },
    },
    {
      "Basic Analogue",
      {
        0.0f,
        0.0f,
        0.0f,
        0.025f,
        0.375f,
        1.0f,
      },
    },
    {
      "Basic Analogue (w/temporal artifacts)",
      {
        0.0f,
        0.0f,
        0.0f,
        0.025f,
        0.375f,
        0.0f,
      },
    },
    {
      "Bad Reception",
      {
        0.25f,
        0.90f,
        3.20f,
        0.25f,
        2.0f,
      },
    },
  };
}