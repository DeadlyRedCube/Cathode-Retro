#pragma once

namespace NTSCify::SignalGeneration
{
  struct ArtifactSettings
  {
    // $TODO These ghosting settings could be better make them better - I think we want one major ghost that is blurred instead of a weird horizontal smudge
    float ghostVisibility = 0.0f;           // How visible is the ghost
    float ghostSpreadScale = 0.650f;        // How far does each sample of the ghost spread from the last
    float ghostDistance    = 4.0f;          // How far from center is the ghost's center

    float noiseStrength = 0.0f;             // How much noise is added to the signal

    float instabilityScale = 0.0f;          // How much horizontal wobble to have on the screen per scanline

    float temporalArtifactReduction = 0.0f; // How much to blend between two different phases to reduce temporal aliasing
  };
}