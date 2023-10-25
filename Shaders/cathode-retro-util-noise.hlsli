#ifndef __NOISE__HLSLI__
  #define __NOISE__HLSLI__


  float Noise2D(float2 coord, float iseed)
  {
    float fseed = frac(iseed / 10000.0);
    float angle = frac(distance(coord.xy, 1000.0 * (float2(fseed + 0.3, 0.1) + 1.0)));
    return frac(tan(angle) * distance(coord.xy, 1000.0 * (float2(fseed + 0.1, fseed + 0.2) - 2.0)));
  }


  float Noise1D(float coord, float iseed)
  {
    return Noise2D(float2(coord, 0.0), iseed);
  }
#endif