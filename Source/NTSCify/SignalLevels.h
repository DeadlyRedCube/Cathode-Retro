#pragma once

namespace NTSCify
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