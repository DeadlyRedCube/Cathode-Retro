#pragma once

namespace NTSCify::Internal
{
  struct SignalLevels
  {
    float temporalArtifactReduction;
    bool isDoubled;       // This can be true if we're doing temporal aliasing reduction - the texture has 2 versions of this frame with 
                          //  different temporal aliasing that can be blended. It's only true if this frame and the previous frame had
                          //  different signal phases (otherwise there are no artifacts to reduce)
    float whiteLevel;
    float blackLevel;
    float saturationScale;
  };
}