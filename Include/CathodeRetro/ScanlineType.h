#pragma once

namespace CathodeRetro
{
  enum class ScanlineType
  {
    Odd,                // This is an "odd" interlaced frame, the (1-based) odd scanlines will be full brightness.
    Even,               // This is an "even" interlaced frame

    Progressive = Odd,  // If doing Progressive scan, you'll want to set scanlineStrength to 0, and always use Odd frames.
  };
}