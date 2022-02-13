Texture2D<float4> g_sourceTexture : register(t0);

sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  float g_gamma;
}


// This shader just takes a texture that's in YIQ color space and converts it into RGB.
float4 main(float2 texCoord: TEX): SV_TARGET
{
	float3 YIQ = g_sourceTexture.SampleLevel(g_sampler, texCoord, 0).rgb;

  // $TODO: These values need to be gamma-adjusted since what is sent over NTSC is in a different gamma space than what is actually displayed.

  // Do the YIQ to RGB conversion and then gamma-adjust it
  float3x3 mat = float3x3(
    1.0,       1.0,       1.0,
    0.946882, -0.274788, -1.108545,
    0.623557, -0.635691,  1.7090047);
  float3 RGB = pow(saturate(mul(YIQ, mat)), g_gamma);

  return float4(RGB, 1);
}
