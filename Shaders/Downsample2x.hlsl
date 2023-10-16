///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Downsample an input image by 2x along a given axis by using a lanczos filter.

#include "ntsc-util-lang.hlsli"
#include "Lanczos2.hlsli"

DECLARE_TEXTURE2D(g_sourceTexture);
DECLARE_SAMPLER(g_sampler);

CBUFFER consts
{
  // The direction that we're downsampling along. Should either be (1, 0) to downsample to a half-width texture or (0, 1) to downsample
  //  to a half-height texture.
  float2 g_filterDir;
};


float4 Main(float2 inTexCoord)
{
  return Lanczos2xDownsample(
    PASS_TEXTURE2D_AND_SAMPLER_PARAM(g_sourceTexture, g_sampler),
    inTexCoord,
    g_filterDir);
}

PS_MAIN;