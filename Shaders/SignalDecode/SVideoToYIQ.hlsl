Texture2D<float4> g_sourceTexture : register(t0);
Texture2D<float2> g_scanlinePhases : register(t1);
RWTexture2D<float4> g_outputTexture : register(u0);


cbuffer consts : register(b0)
{
  uint g_samplesPerColorburstCycle;
  float g_tint;
  float g_saturation;
  float g_brightness;
  float g_blackLevel;
  float g_whiteLevel;
  float g_temporalArtifactReduction;
}

static const float pi = 3.1415926535897932384626433832795028841971f;


// This shader demodulates the chroma information to get the I and Q color space values out of it (Y is easy, we already have it, it's just the
//  luma value).
//
// This process is called NTSC color demodulation, which is a type of QAM demodulation.
// Just like filtering the luma from the chroma (see the comment in CompositeToSVideo.hlsl), rather than doing any sort of fancy filtering here,
// the absolute best results I've gotten were from a super-basic average (which very nicely removes a very specific frequency and all integer
//  multiples of it), which is also exactly what we need here.
//
// Additionally, if we want temporal artifact reduction, we're doing this whole calculation twice (we have a float4(lumaA, chromaA, lumaB, chromaB) 
//  texture representing the same frame with two different color phases), and we use the temporal artifact reduction value to blend between them,
//  which eliminates alternating-phase-related flickering that systems like the NES could generate.
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // We're going to sample chroma at double our actual colorburst cycle to eliminate some deeply rough artifacting on the edges.
  //  In this case we're going to average by DOUBLE the color burst cycle - doing just a single cycle ends up with a LOT of edge artifacting on
  //  things, which are too strong. 2x is much softer but not so large that you can really notice that it's extra.
  uint filterWidth = g_samplesPerColorburstCycle * 2;

  int leftX = -int((filterWidth - 1) / 2);

  float2 relativePhase = g_scanlinePhases.Load(uint3(dispatchThreadID.y, 0, 0)) + g_tint;

  // This is the chroma decode process, it's a QAM demodulation. 
  //  You multiply the chroma signal by a reference waveform and its quadrature (Basically, sin and cos at a given time) and then filter
  //  out the chroma frequency (here done by a box filter (an average)). What you're left with are the approximate I and Q color space
  //  values for this part of the image.
  float4 IQ = (0).xxxx;
  for (uint i = 0; i < filterWidth; i++)
  {
    float2 chroma = g_sourceTexture.Load(uint3(uint2(leftX + i, 0) + dispatchThreadID, 0)).yw;
    float2 s, c;
    sincos(2.0 * pi * (0.5 / float(g_samplesPerColorburstCycle) + float(leftX + i + dispatchThreadID.x) / g_samplesPerColorburstCycle + relativePhase), s, c);

    IQ += chroma.xxyy * float4(s, -c).xzyw;
  }

  IQ /= filterWidth;

  float2 Y = g_sourceTexture.Load(uint3(dispatchThreadID, 0)).xz;

  // Adjust our components, first Y to account for the signal's black/white level (and user-chosen brightness), then IQ for saturation (Which includes the
  //  signal's saturation scale)
  Y = (Y - g_blackLevel) / (g_whiteLevel - g_blackLevel) * g_brightness;
  IQ *= g_saturation.xxxx;

  Y.x = lerp(Y.x, Y.y, g_temporalArtifactReduction * 0.5);
  IQ.xy = lerp(IQ.xy, IQ.zw, g_temporalArtifactReduction * 0.5);

  g_outputTexture[dispatchThreadID] = float4(Y.x, IQ.xy, 1);
}