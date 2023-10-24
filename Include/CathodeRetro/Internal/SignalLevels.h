#pragma once

namespace CathodeRetro
{
  namespace Internal
  {
    struct SignalLevels
    {
      float temporalArtifactReduction;
      float whiteLevel;
      float blackLevel;
      float saturationScale;
    };
  }
}