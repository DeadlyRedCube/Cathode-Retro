#pragma once

#include "BaseTypes.h"

namespace NTSC
{
  // Wang Hash complements of Nathan Reed @ https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
  inline f32 WangHashAndXorShift(u32 seed)
  {
    // wang hash
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);

    // xorshift
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return f32(f64(seed) * (1.0 / 4294967296.0));
  }
}