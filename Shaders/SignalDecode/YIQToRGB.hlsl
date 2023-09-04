///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader converts an input texture that is in NTSC-standard YIQ color space and converts it into RGB color space.


// This is the input YIQ texture. It's expected to be the same resolution as our output render target.
Texture2D<float4> g_sourceTexture : register(t0);

// This sampler doesn't have any specific sampling/addressing needs, as we're always going to be sampling at texel centers given the
//  matching input/output texture dimensions. I tend to use an existing linear sampling/clamp addressing sampler that already exist.
sampler g_sampler : register(s0);

float4 main(float2 texCoord: TEX): SV_TARGET
{
	float3 yiq = g_sourceTexture.Sample(g_sampler, texCoord).rgb;

  // This is the NTSC standard (SMPTE C) YIQ to RGB conversion (from https://en.wikipedia.org/wiki/YIQ)
  float3 rgb;
  rgb.r = dot(yiq, float3(1.0, 0.946882, 0.623557));
  rgb.g = dot(yiq, float3(1.0, -0.274788, -0.635691));
  rgb.b = dot(yiq, float3(1.0, -1.108545, 1.7090047));
  return float4(rgb, 1);
}
