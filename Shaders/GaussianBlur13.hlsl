///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader does a 1D gaussian blur using a 13-tap filter.


#include "ntsc-util-lang.hlsli"


// 13-tap gaussian kernel coefficients for bilinear shading, optimized to only require 7 texture samples by taking advantage of linear
//  texture filtering. These coefficients were generated using https://drilian.com/gaussian-kernel/

/*
  The actual coefficients for this blur are:

  1.107819053e-2
  2.478669128e-2
  4.790462288e-2
  7.997337680e-2
  1.153247662e-1
  1.436510723e-1
  1.545625599e-1
  1.436510723e-1
  1.153247662e-1
  7.997337680e-2
  4.790462288e-2
  2.478669128e-2
  1.107819053e-2
*/


DECLARE_TEXTURE2D(g_sourceTex, g_sampler);

CONST int k_sampleCount = 7;
BEGIN_CONST_ARRAY(float, k_coeffs, k_sampleCount)
  3.586488181e-2,
  1.278779997e-1,
  2.589758386e-1,
  1.545625599e-1,
  2.589758386e-1,
  1.278779997e-1,
  3.586488181e-2
END_CONST_ARRAY

BEGIN_CONST_ARRAY(float, k_offsets, k_sampleCount)
  -5.308886854,
  -3.374611919,
  -1.445310910,
  0.000000000,
  1.445310910,
  3.374611919,
  5.308886854
END_CONST_ARRAY

// Blur a texture along the blur direction (for a horizontal blur, use (1, 0) and for vertical use (0, 1)), centered at "centerTexCoord"
//  (which is in standard [0..1] texture space).
float4 Blur(float2 centerTexCoord, float2 blurDirection)
{
  int2 dim;
  GET_TEXTURE_SIZE(g_sourceTex, dim);

  float4 v = float4(0, 0, 0, 0);
  for (int i = 0; i < k_sampleCount; i++)
  {
    float2 c = centerTexCoord + blurDirection / float2(dim) * k_offsets[i];
    v += SAMPLE_TEXTURE(g_sourceTex, g_sampler, c) * k_coeffs[i];
  }

  return v;
}


CBUFFER consts
{
  // The direction to blur along. Should be (1, 0) to do a horizontal blur and (0, 1) to do a vertical blur.
  float2 g_blurDir;
};


float4 Main(float2 inTexCoord)
{
  return Blur(inTexCoord, g_blurDir);
}


PS_MAIN