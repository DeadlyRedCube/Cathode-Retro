#pragma once

namespace NTSCify
{
  struct TVKnobSettings
  {
    float saturation = 1.0f;    // The saturation of the decoded signal (where 1.0 is the default)
    float brightness = 1.0f;    // The brightnesss of the decoded signal
    float tint = 0.0f;          // An additional tint to apply to the output signal
    float sharpness = 0.0f;     // 0 is unfiltered, 1 is "fully sharpened", -1 is "fully blurred"
  };
}