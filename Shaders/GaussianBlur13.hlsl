// 13-tap gaussian kernel coefficients for bilinear shading, generated using https://drilian.com/gaussian-kernel/

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


Texture2D<unorm float4> g_sourceTex;
sampler g_sampler;

static const uint k_sampleCount = 7;
static const float k_coeffs[k_sampleCount] =
{ 
  3.586488181e-2,
  1.278779997e-1,
  2.589758386e-1,
  1.545625599e-1,
  2.589758386e-1,
  1.278779997e-1,
  3.586488181e-2,
};

static const float k_offsets[k_sampleCount] =
{ 
  -5.308886854,
  -3.374611919,
  -1.445310910,
  0.000000000,
  1.445310910,
  3.374611919,
  5.308886854,
};

// Blur a texture along the blur direction (for a horizontal blur, use [1, 0] and for vertical use [0, 1]), centered at "centerTexCoord"
//  (which is in standard [0..1] texture space).
float4 Blur(float2 centerTexCoord, float2 blurDirection)
{
  uint2 dim;
  g_sourceTex.GetDimensions(dim.x, dim.y);

  float4 v = float4(0, 0, 0, 0);
  for (uint i = 0; i < k_sampleCount; i++)
  {
    float2 c = centerTexCoord + blurDirection / float2(dim) * k_offsets[i];
    v += g_sourceTex.Sample(g_sampler, c) * k_coeffs[i];
  }

  return v;
} 


cbuffer consts : register(b0)
{
  float2 g_blurDir;
}


float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  return Blur(inTexCoord, g_blurDir);
}