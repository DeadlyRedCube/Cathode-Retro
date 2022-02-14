#include "../TrackingInstability.hlsli"

Texture2D<float4> g_sourceTexture : register(t0);
Texture2D<float2> g_scanlinePhases : register(t1);

cbuffer consts : register (b0)
{
  uint g_outputTexelsPerColorburstCycle;

  uint g_inputWidth;
  uint g_outputWidth;

  uint g_scanlineCount;

  float g_compositeBlend;

  float g_instabilityScale;
  uint g_noiseSeed;
}


static const float pi = 3.1415926535897932384626433832795028841971f;

sampler g_sampler : register(s0);

// This shader takes an RGB image and turns it into either an SVideo or Composite signal (Based on whether g_compositeBlend is 0 or 1).
//  We might also be generating a PAIR of these, using two different sets of phase inputs (if g_scanlinePhases is a two-component input),
//  for purposes of temporal aliasing reduction.
float4 main(float2 signalTexCoord: TEX): SV_TARGET
{
  uint2 signalTexelIndex = uint2(round(signalTexCoord * float2(g_outputWidth, g_scanlineCount) - 0.5));
  float2 texCoord = (float2(signalTexelIndex) * float2(float(g_inputWidth) / float(g_outputWidth), 1) + 0.5) / float2(g_inputWidth, g_scanlineCount);

  float instability = CalculateTrackingInstabilityOffset(
    signalTexelIndex.y, 
    g_scanlineCount,
    g_noiseSeed,
    g_instabilityScale,
    g_outputWidth);

  texCoord.x += instability;

  float3 RGB = g_sourceTexture.SampleLevel(g_sampler, texCoord, 0).rgb; 
  
  float3x3 mat = float3x3(
    0.3000,  0.5990,  0.2130,  // r
    0.5900, -0.2773, -0.5251,  // g
    0.1100, -0.3217,  0.3121); // b

  // Convert RGB to YIQ
  float3 YIQ = mul(RGB, mat);

  // Do some gamma adjustments to counter for the gamma that will be used at decode time.
  YIQ.x = pow(saturate(YIQ.x), 2.2 / 2);
  float iqSat = saturate(length(YIQ.yz));
  YIQ.yz *= pow(iqSat, 2.2 / 2) / max(0.00001, iqSat);
  
  // Separate them out because YIQ.y ended up being confusing (is it y? no it's i! but also it's y!)
  float Y = YIQ.x;
  float I = YIQ.y;
  float Q = YIQ.z;

  // Figure out where in the carrier wave we are
  float2 scanlinePhase = g_scanlinePhases.SampleLevel(g_sampler, float2(signalTexelIndex.y + 0.5, 0) / g_scanlineCount, 0);

  // Calculate the phase
  float2 phase = scanlinePhase + (signalTexelIndex.x) / float(g_outputTexelsPerColorburstCycle);

  // Now we need to encode our IQ component in the carrier wave at the correct phase
  float2 s, c;
  sincos(2.0 * pi * phase, s, c);

  float2 luma = Y.xx;
  float2 chroma = s * I - c * Q;

  // If compositeBlend is 1, this is a composite output and we combine the whole signal into the one channel. Otherwise, use two.
  if (g_compositeBlend > 0)
  {
    return (luma + chroma).xyxy;
  }
  else
  {
    return float4(luma, chroma).xzyw;
  }
}