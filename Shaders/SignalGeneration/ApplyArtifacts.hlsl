#include "../Noise.hlsli"

Texture2D<float4>  g_sourceTexture : register(t0);

sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  float g_ghostSpreadScale;
  float g_ghostBrightness;
  float g_ghostDistance;
  int g_noiseSeed;
  float g_noiseStrength;
  int g_signalTextureWidth;
  int g_scanlineCount;
  uint g_samplesPerColorburstCycle;
  uint g_hasDoubledSignal;
}


// This shader applies ghosting and noise to an SVideo or Composite signal.
float4 main(float2 inputTexCoord: TEX): SV_TARGET
{
  uint2 pixelIndex = uint2(round(inputTexCoord * float2(g_signalTextureWidth, g_scanlineCount) - 0.5));

  float4 lumaChroma = g_sourceTexture.Sample(g_sampler, inputTexCoord);

  // Ghosting basically is what happens when a copy of your signal "skips" its intended path through the cable and mixes
  //  in with your normal signal (like an EM leak of the signal) and is basically a pre-echo of the signal. So just
  //  take the signal and add a pre-echoed version of it
  if (g_ghostBrightness != 0)
  {
    float4 ghost = (0).xxxx;

    float2 ghostCenterCoord = inputTexCoord + float2(g_ghostDistance * g_samplesPerColorburstCycle, 0) / float2(g_signalTextureWidth, g_scanlineCount);

    float2 ghostSampleSpread = float2(g_ghostSpreadScale * g_samplesPerColorburstCycle, 0)  / float2(g_signalTextureWidth, g_scanlineCount);

    // The following is a 9-tap gaussian, written as 5 samples using bilinear interpolation to approximate two at once:
    // 0.00761441700, 0.0360749699, 0.109586075, 0.213444546, 0.266559988, 0.213444546, 0.109586075, 0.0360749699, 0.00761441700

    ghost += g_sourceTexture.Sample(g_sampler, ghostCenterCoord - ghostSampleSpread * 1.174285279339) * 0.0436893869;
    ghost += g_sourceTexture.Sample(g_sampler, ghostCenterCoord - ghostSampleSpread * 1.339243613069) * 0.323030611;
    ghost += g_sourceTexture.Sample(g_sampler, ghostCenterCoord)                                      * 0.266559988;
    ghost += g_sourceTexture.Sample(g_sampler, ghostCenterCoord + ghostSampleSpread * 1.339243613069) * 0.323030611;
    ghost += g_sourceTexture.Sample(g_sampler, ghostCenterCoord + ghostSampleSpread * 1.174285279339) * 0.0436893869;

    lumaChroma += ghost * g_ghostBrightness;

  }

  // Also add some noise for each texel.
  float noise = 0;
  uint noiseCenter = (g_noiseSeed * g_scanlineCount + pixelIndex.y) * g_signalTextureWidth + pixelIndex.x / 2;
  noise = WangHashAndXorShift(noiseCenter) + (WangHashAndXorShift(noiseCenter + 1) + WangHashAndXorShift(noiseCenter - 1)) * 0.5;
  noise = (noise - 1) * g_noiseStrength * 0.5;
  return lumaChroma + noise;
}