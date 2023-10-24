#pragma once

namespace NTSCify::Internal
{
  struct SignalLevels
  {
    float temporalArtifactReduction;
    float whiteLevel;
    float blackLevel;
    float saturationScale;
  };
}