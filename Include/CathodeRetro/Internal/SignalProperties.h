#pragma once

#include "CathodeRetro/Settings.h"


namespace CathodeRetro
{
  namespace Internal
  {
    struct SignalProperties
    {
      bool operator==(const SignalProperties &other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
      bool operator!=(const SignalProperties &other) const { return memcmp(this, &other, sizeof(*this)) != 0; }

      SignalType type;
      uint32_t scanlineWidth;
      uint32_t scanlineCount;
      float colorCyclesPerInputPixel;
      float inputPixelAspectRatio; // $TODO: Does this really belong here? Need a better aspect ratio wiring but this works for now
    };
  }
}