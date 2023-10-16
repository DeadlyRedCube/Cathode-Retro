// This is a lanczos2 kernel for a nice 2x downsample. I wish I'd documented how I generated it but I didn't so instead this is what you
//  get. Sorry about that.
//static const float k_lanczos2[8] = {-0.009, -0.042, 0.117, 0.434, 0.434, 0.117, -0.042, -0.009};

// However, it's been optimized to take advantage of the linear filtering using the same technique I used for the gaussian filters at
//  https://drilian.com/gaussian-kernel/
// That means it's only 4 texture samples for an 8-tap lanczos, which is, if I did my math correctly, twice as good!

#include "ntsc-util-lang.hlsli"

CONST int k_sampleCount = 4;
BEGIN_CONST_ARRAY(float, k_coeffs, 4)
  -0.051,
   0.551,
   0.551,
  -0.051
END_CONST_ARRAY

BEGIN_CONST_ARRAY(float, k_offsets, 4)
  -2.67647052,
  -0.712341249,
   0.712341189,
   2.67647052
END_CONST_ARRAY


float4 Lanczos2xDownsample(
  DECLARE_TEXTURE2D_AND_SAMPLER_PARAM(sourceTexture, samp),
  float2 centerTexCoord,
  float2 filterDir)
{
  int2 dim;
  GET_TEXTURE_SIZE(sourceTexture, dim);

  float4 v = float4(0.0, 0.0, 0.0, 0.0);
  for (int i = 0; i < k_sampleCount; i++)
  {
    float2 c = centerTexCoord + filterDir / float2(dim) * k_offsets[i];
    v += SAMPLE_TEXTURE(sourceTexture, samp, c) * k_coeffs[i];
  }

  return v;
}