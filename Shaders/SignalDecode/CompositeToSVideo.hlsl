Texture2D<float1> g_sourceTexture : register(t0);
RWTexture2D<float2> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  uint g_samplesPerColorburstCycle;
}

// Use a simple average (box filter) that is the same size as our color burst frequency to filter out the luminance from the chroma.
//  Seriously I tried so many fancy filters and the box filter ended up being both the fastest as well as the most effective filter I could find. 
//  So whatever, it may not be exactly what old hardware did but it looks REALLY close and it's way more efficient than an actual lowpass FIR or IIR.
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // Figure out where the left side of our box filter is
  int leftX = -int((g_samplesPerColorburstCycle - 1) / 2);
  
  // Average the luma samples
  // $TODO make this 2x as efficient by leaning on bilinear filtering instead of doing individual loads
  float luma = 0.0f;
  for (uint i = 0; i < g_samplesPerColorburstCycle; i++)
  {
    luma += g_sourceTexture.Load(uint3(uint2(leftX + i, 0) + dispatchThreadID, 0));
  }

  luma /= float(g_samplesPerColorburstCycle);

  // Now that we have our average luminance, that's our luma, and then subtract it from our center sample to get the separated chroma information.
  float centerSample = g_sourceTexture.Load(uint3(dispatchThreadID, 0));
  g_outputTexture[dispatchThreadID] = float2(luma, centerSample - luma);
}