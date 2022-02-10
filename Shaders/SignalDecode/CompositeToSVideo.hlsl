Texture2D<float2> g_sourceTexture : register(t0);
RWTexture2D<float4> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  uint g_samplesPerColorburstCycle;
}

// This shader takes a composite signal (like you'd get via a console's composite input) and separates out the luma chroma so it is effectively an SVideo 
//  signal. I tried all sorts of wacky hijinks to filter these things out in a solid emulation of what old hardware was doing, but all of those filters 
//  were both VERY expensive and also had annoying artifacts (like ringing). tried all sorts of low pass and band stop solutions to get the luma separated
//  from the chroma.
//
// However, it turns out, that all you REALLY need to do (if your color cycle frequency is a nice multiple of your pixel resolution, which it is), is
//  average out that many pixels' worth of signal, and that flattens out the wave frequency very nicely, leaving you with just the luma.
//
//  Basically, if you average every sample of the below sine wave, what you end up with is a value that represents the y coordinate of its center (the
//   line in the diagram)
//         _....._
//      ,="       "=.
//    ,"             ".
//   /                 ".
//   ---------------------.-------------------/
//                         ".               ."
//                           "._         _,"
//                              "-.....-"
//
// Once you have that, you can simply subtract that value from the original signal and you're left with JUST the wave information (which is the chroma).
//  Basically, months of trying to figure out the best way to do something turned into "average these few texels together, the end"
//
// It's also worth noting that this is doing TWO luma/chroma demodulations at once, for purposes of temporal aliasing reduction.
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // Figure out where the left side of our box filter is (keeping it aligned to a texel boundary - this means our filter is actually off by 1/2 a
  //  texel (since it's an even number of texels and thus doesn't have a true center texel), so we COULD make it more centered by doing one additional
  //  sample at the end and having the end samples be half-intensity. $TODO Probably just go ahead and do this when you get around to bilinear filtering
  //  the filter to make it more efficient
  int leftX = -int((g_samplesPerColorburstCycle - 1) / 2);
  
  // Average the luma samples
  // $TODO make this 2x as efficient by leaning on bilinear filtering instead of doing individual loads
  float2 luma = 0.0f;
  for (uint i = 0; i < g_samplesPerColorburstCycle; i++)
  {
    luma += g_sourceTexture.Load(uint3(uint2(leftX + i, 0) + dispatchThreadID, 0));
  }

  luma /= float(g_samplesPerColorburstCycle);

  // Now that we have our average signal value, that's our luma, and then subtract it from our center sample to get the isolated chroma information.
  float2 centerSample = g_sourceTexture.Load(uint3(dispatchThreadID, 0));
  g_outputTexture[dispatchThreadID] = float4(luma, centerSample - luma).xzyw;
}