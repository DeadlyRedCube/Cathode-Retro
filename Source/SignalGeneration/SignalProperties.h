#pragma once

#include "SourceSettings.h"


namespace NTSCify::SignalGeneration
{
  struct SignalProperties
  {
    SignalType type;
    uint32_t scanlineWidth;
    uint32_t scanlineCount;
    float colorCyclesPerInputPixel;
  };
}