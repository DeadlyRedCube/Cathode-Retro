#pragma once

namespace NTSCify::SignalDecode
{
  struct TVKnobSettings
  {
    float saturation = 1.0f;
    float brightness = 1.0f;
    float gamma = 1.0f;
    float tint = 0.0f;
    float sharpness = 0.0f;   // 0 is basically unfiltered, 1 is "fully sharpened", -1 is "fully blurred"
  };
}