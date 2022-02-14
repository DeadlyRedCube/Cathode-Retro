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
}