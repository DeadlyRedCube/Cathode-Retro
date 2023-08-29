#pragma once

namespace NTSCify
{
  enum class ScanlineType
  {
    Progressive,     // Input scanline count is 1:1 with screen scanline count
    FauxProgressive, // Input scanline count is 1:2 with screen scanline count but the fields don't move (same positionas "Odd")
    Odd,             // This is an "odd" interlaced frame, the (1-based) odd scanlines will be full brightness.
    Even,            // This is an "even" interlaced frame
  };
}