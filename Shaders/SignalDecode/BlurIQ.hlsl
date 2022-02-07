Texture2D<float4> g_sourceTexture : register(t0);
Texture2D<float> g_kernelTexture : register(t1);
RWTexture2D<float4> g_outputTexture : register(u0);


cbuffer consts : register(b0)
{
  uint g_kernelSize;
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID )
{
  int leftX = -int((g_kernelSize - 1) / 2);
  
  // $TODO Make this twice as efficient by leaning on bilinear filtering and doing half the fetches
  float Y = g_sourceTexture.Load(uint3(dispatchThreadID, 0)).x;
  float2 IQ = float2(0, 0);
  for (uint i = 0; i < g_kernelSize; i++)
  {
    IQ += g_sourceTexture.Load(uint3(uint2(leftX + i, 0) + dispatchThreadID, 0)).yz * g_kernelTexture.Load(uint3(i, 0, 0)).x;
  }

  g_outputTexture[dispatchThreadID] = float4(Y, IQ, 1);
}
