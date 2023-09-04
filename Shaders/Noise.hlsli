#ifndef __NOISE__HLSLI__
#define __NOISE__HLSLI__

// WangHash + XorShift used to generate noise to add to the signal.
//  Thanks to Nathan Reed, at http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
float WangHashAndXorShift(uint seed)
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
  return float(seed) * (1.0 / 4294967296.0);
}

#endif