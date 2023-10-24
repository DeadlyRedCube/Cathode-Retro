#include "cathode-retro-util-noise.hlsli"

// Calculate the instability noise per scanline
float CalculateTrackingInstabilityOffset(uint scanlineIndex, uint scanlineCount, uint noiseSeed, float scale, uint signalTextureWidth)
{
  float noise = 0.0;
  {
    noise = WangHashAndXorShift(uint(noiseSeed * scanlineCount  + scanlineIndex)) - 0.5;
    noise *= scale / signalTextureWidth;
  }

  return noise;
}