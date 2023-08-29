#pragma once

#include <cinttypes>

namespace NTSCify
{
  struct OverscanSettings
  {
    bool operator==(const OverscanSettings &other) const = default;

    // Each of these represents how many input pixels of overscan there is on each side of the view
    uint32_t overscanLeft = 0;
    uint32_t overscanRight = 0;
    uint32_t overscanTop = 0;
    uint32_t overscanBottom = 0;
  };
}