#pragma once

namespace CathodeRetro::Internal
{
  struct SignalLevels
  {
    float temporalArtifactReduction;
    float whiteLevel;
    float blackLevel;
    float saturationScale;
  };
}