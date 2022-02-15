#pragma once

#include "SourceSettings.h"


namespace NTSCify
{
  struct SignalProperties
  {
    SignalType type;
    uint32_t scanlineWidth;
    uint32_t scanlineCount;
    float colorCyclesPerInputPixel;
    float inputPixelAspectRatio; // $TODO: Does this really belong here? Need a better aspect ratio wiring but this works for now
  };
}