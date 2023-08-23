Texture2D<float4> g_sourceTexture : register(t0);

sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  uint g_isOddFrame;
  float g_scanlineStrength;
}

// This shader just takes a texture that's in YIQ color space and converts it into RGB.
float4 main(float2 texCoord : TEX, float4 m_position : SV_Position) : SV_TARGET
{
  float3 color = g_sourceTexture.Sample(g_sampler, texCoord).rgb;

  if ((uint(m_position.y) & 1) == g_isOddFrame)
  {
    color *= (1.0 - g_scanlineStrength);
  }

  return float4(color, 1);
}
