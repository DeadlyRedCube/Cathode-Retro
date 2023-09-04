///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader does a tonemap and 1D downsample of a texture, which is intended to be for the diffusion emulation in the CRT side of the
//  whole NTSCify process. It's worth noting that in practice we're not doing a true 2x downsample, it's going to be something in that
//  ballpark, but not exact. However, the output of this shader is going to be blurred so it doesn't actually matter in practice.


#include "Lanczos2.hlsli"


Texture2D<unorm float4> g_sourceTexture : register(t0);
sampler g_sampler : register(s0);


cbuffer consts : register(b0)
{
  // The direction we want to apply the downsample.
  float2 g_downsampleDir;
  float g_minLuminosity;

  float g_colorPower;
}


float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  float4 samp = Lanczos2xDownsample(g_sourceTexture, g_sampler, inTexCoord, g_downsampleDir);

  // Calculate the luminosity of the input.
  float inLuma = dot(samp.rgb, float3(0.30, 0.59, 0.11));

  // Calculate the desired output luminosity.
  float outLuma = (inLuma - g_minLuminosity) / (1.0 - g_minLuminosity);
  outLuma = pow(saturate(outLuma), g_colorPower);

  // Apply the luminosity scaling.
  samp.rgb *= outLuma / inLuma;
  return samp;
}