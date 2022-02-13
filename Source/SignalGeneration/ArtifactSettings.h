#pragma once

namespace NTSCify::SignalGeneration
{
  struct ArtifactSettings
  {
    // $TODO These ghosting settings could be better make them better - I think we want one major ghost that is blurred instead of a weird horizontal smudge
    float ghostVisibility = 0.0f;           // How visible is the ghost
    float ghostSpreadScale = 0.650f;        // 
    float ghostDistance = 7.0f / 8.0f;      // 

    float noiseStrength = 0.0f;             // How much noise is added to the signal

    float instabilityScale = 4.0f;          // How much horizontal wobble to have on the screen per scanline

    float temporalArtifactReduction = 1.0f; // How much to blend between two different phases to reduce temporal aliasing
  };
}