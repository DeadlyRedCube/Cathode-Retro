#include "../Noise.hlsli"

Texture2D<float4>  g_sourceTexture : register(t0);
RWTexture2D<float4> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  float g_ghostSpreadScale;
  float g_ghostBrightness;
  int g_noiseSeed;
  float g_noiseScale;
  int g_signalTextureWidth;
  int g_scanlineCount;
}


// This shader applies ghosting and noise to an SVideo or Composite signal.
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  float4 lumaChroma = g_sourceTexture.Load(uint3(dispatchThreadID, 0));

  // $TODO This ghosting implementation isn't right, a better one would be to have an offset blurry pre-echo.
  // Ghosting basically is what happens when a copy of your signal "skips" its intended path through the cable and mixes
  //  in with your normal signal (like an EM leak of the signal) and is basically a pre-echo of the signal. So just 
  //  take the signal and add a pre-echoed version of it (And scale the signal down to compensate)
  if (g_ghostBrightness > 0)
  {
    int ghostDelta = 9;
    float4 ghost = (0).xxxx;

    float ghostSpreadScale2 = g_ghostSpreadScale * g_ghostSpreadScale;
    float ghostSpreadScale3 = ghostSpreadScale2 * g_ghostSpreadScale;
    float ghostSpreadScale4 = ghostSpreadScale2 * ghostSpreadScale2;

    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta, 0), 0)) * 1;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 2, 0), 0)) * g_ghostSpreadScale;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 3, 0), 0)) * ghostSpreadScale2;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 4, 0), 0)) * ghostSpreadScale3;
    ghost += g_sourceTexture.Load(uint3(dispatchThreadID + int2(ghostDelta * 5, 0), 0)) * ghostSpreadScale4;

    ghost /= (1 + g_ghostSpreadScale + ghostSpreadScale2 + ghostSpreadScale3 + ghostSpreadScale4);

    float ghostBrightness = 0.5;
    lumaChroma += ghost * g_ghostBrightness;
    lumaChroma /= 1 + g_ghostBrightness;
  }

  // Also add some noise for each texel.
  float noise = 0;
  noise = (WangHashAndXorShift((g_noiseSeed * g_scanlineCount + dispatchThreadID.y) * g_signalTextureWidth + dispatchThreadID.x) - 0.5) * g_noiseScale;
  g_outputTexture[dispatchThreadID] = lumaChroma + noise;
}