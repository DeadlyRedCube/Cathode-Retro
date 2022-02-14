#pragma once

namespace NTSCify::SignalGeneration
{
  struct SignalLevels
  {
    bool isDoubled;

    float whiteLevel;
    float blackLevel;
    float saturationScale;
    
    float temporalArtifactReduction;
  };
}