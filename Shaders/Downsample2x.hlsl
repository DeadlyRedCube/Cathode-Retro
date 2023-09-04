///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Downsample an input image by 2x along a given axis by using a lanczos filter.


#include "Lanczos2.hlsli"

Texture2D<unorm float4> g_sourceTexture : register(t0);
sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  // The direction that we're downsampling along. Should either be (1, 0) to downsample to a half-width texture or (0, 1) to downsample
  //  to a half-height texture.
  float2 g_filterDir;
}


float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  return Lanczos2xDownsample(g_sourceTexture, g_sampler, inTexCoord, g_filterDir);
}