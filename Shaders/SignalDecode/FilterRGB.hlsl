Texture2D<float4> g_sourceTexture : register(t0);

sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  float g_blurStrength;
  float g_stepSize;
}

float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y);  

  // Just a simple three-tap 1D filter.
  float blurSide = g_blurStrength / 3.0;
  float blurCenter = 1.0 - 2.0 * blurSide;
  float4 color = g_sourceTexture.Sample(g_sampler, inTexCoord - float2(g_stepSize, 0) * inputTexelSize) * blurSide
               + g_sourceTexture.Sample(g_sampler, inTexCoord)                                          * blurCenter
               + g_sourceTexture.Sample(g_sampler, inTexCoord + float2(g_stepSize, 0) * inputTexelSize) * blurSide;
  
  return color;
}
