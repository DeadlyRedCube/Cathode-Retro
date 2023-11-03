///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader does a tonemap and 1D downsample of a texture, which is intended to be for the diffusion emulation in
//  the CRT side of the whole Cathode Retro process. It's worth noting that in practice we're not doing a true 2x
//  downsample, it's going to be something in that ballpark, but not exact. However, the output of this shader is going
//  to be blurred so it doesn't actually matter in practice.


#include "cathode-retro-util-lanczos.hlsli"


DECLARE_TEXTURE2D(g_sourceTexture, g_sampler);


CBUFFER consts
{
  // The direction we want to apply the downsample.
  float2 g_downsampleDir;
  float g_minLuminosity;

  float g_colorPower;
};


float4 Main(float2 inTexCoord)
{
  float4 samp = Lanczos2xDownsample(
    PASS_TEXTURE2D_AND_SAMPLER_PARAM(g_sourceTexture, g_sampler),
    inTexCoord,
    g_downsampleDir);

  // Calculate the luminosity of the input.
  float inLuma = dot(samp.rgb, float3(0.30, 0.59, 0.11));

  // Calculate the desired output luminosity.
  float outLuma = (inLuma - g_minLuminosity) / (1.0 - g_minLuminosity);
  outLuma = pow(saturate(outLuma), g_colorPower);

  // Apply the luminosity scaling.
  samp.rgb *= outLuma / inLuma;
  return samp;
}

PS_MAIN