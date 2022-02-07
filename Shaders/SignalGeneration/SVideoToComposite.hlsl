Texture2D<float2>  g_sourceTexture : register(t0);
RWTexture2D<float> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  float g_ghostSpreadScale;
  float g_ghostBrightness;
  int g_noiseSeed;
  float g_noiseScale;
  int g_signalTextureWidth;
  int g_scanlineCount;
}

// WangHash+XorShift used to generate noise to add to the signal. Thanks to Nathan Reed (http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/)
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


[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // Simple shader, just combine the two into a single composite signal
  float2 lumaChroma = g_sourceTexture.Load(uint3(dispatchThreadID, 0));

#if 1
  if (g_ghostBrightness > 0)
  {
    int ghostDelta = 9;
    float2 ghost = float2(0,0);

    float ghostSpreadScale2 = g_ghostSpreadScale * g_ghostSpreadScale;
    float ghostSpreadScale3 = ghostSpreadScale2 * g_ghostSpreadScale;
    float ghostSpreadScale4 = ghostSpreadScale2 * ghostSpreadScale2;

    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta, 0), 0)) * 1;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 2, 0), 0)) * g_ghostSpreadScale;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 3, 0), 0)) * ghostSpreadScale2;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 4, 0), 0)) * ghostSpreadScale3;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 5, 0), 0)) * ghostSpreadScale4;

    ghost /= (1 + g_ghostSpreadScale + ghostSpreadScale2 + ghostSpreadScale3 + ghostSpreadScale4);

    ghost.y *= g_ghostSpreadScale;

    float ghostBrightness = 0.5;
    lumaChroma += ghost * g_ghostBrightness;
    lumaChroma.x /= 1 + g_ghostBrightness;
  }
#endif

  float noise = 0;
#if 1
  noise = (WangHashAndXorShift((g_noiseSeed * g_scanlineCount + dispatchThreadID.y) * g_signalTextureWidth + dispatchThreadID.x) - 0.5) * g_noiseScale;
#endif
  g_outputTexture[dispatchThreadID] = lumaChroma.x + lumaChroma.y + noise;
}