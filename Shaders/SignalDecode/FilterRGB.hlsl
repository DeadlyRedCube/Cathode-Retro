Texture2D<float4> g_sourceTexture : register(t0);
RWTexture2D<float4> g_outputTexture : register(u0);

sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  float g_blurStrength;
  int   g_texelStepSize;
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID )
{
  float2 inputTexDim;
  g_sourceTexture.GetDimensions(inputTexDim.x, inputTexDim.y);

  // Just a simple three-tap 1D filter.
  float blurSide = g_blurStrength / 3.0;
  float blurCenter = 1.0 - 2.0 * blurSide;
  float4 color = g_sourceTexture.SampleLevel(g_sampler, (0.5 + dispatchThreadID - float2(g_blurStrength, 0)) / inputTexDim, 0) * blurSide
               + g_sourceTexture.SampleLevel(g_sampler, (0.5 + dispatchThreadID) / inputTexDim,                             0) * blurCenter
               + g_sourceTexture.SampleLevel(g_sampler, (0.5 + dispatchThreadID + float2(g_blurStrength, 0)) / inputTexDim, 0) * blurSide;
  g_outputTexture[dispatchThreadID] = color;
}
