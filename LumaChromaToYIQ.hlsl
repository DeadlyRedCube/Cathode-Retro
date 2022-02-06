Texture2D<float2> g_sourceTexture : register(t0);
Texture2D<float> g_scanlinePhases : register(t1);
RWTexture2D<float4> g_outputTexture : register(u0);


cbuffer consts : register(b0)
{
  uint g_samplesPerColorburstCycle;
  float g_tint;
}

static const float pi = 3.1415926535897932384626433832795028841971f;
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // We're going to sample chroma at double our actual colorburst cycle to eliminate some deeply rough artifacting on the edges
  uint filterWidth = g_samplesPerColorburstCycle * 2;

  int leftX = -int((filterWidth - 1) / 2);

  float relativePhase = g_scanlinePhases.Load(uint3(dispatchThreadID.y, 0, 0)).x; // + g_tint;

  // This is the chroma decode process, it's a QAM demodulation. 
  //  You multiply the chroma signal by a reference waveform and its quadrature (Basically, sin and cos at a given time) and then filter
  //  out the chroma frequency (here done by a box filter (an average)). What you're left with are the approximate I and Q color space
  //  values for this part of the image.
  float2 IQ = float2(0, 0);
  for (uint i = 0; i < filterWidth; i++)
  {
    float chroma = g_sourceTexture.Load(uint3(uint2(leftX + i, 0) + dispatchThreadID, 0)).y;
    float s, c;
    sincos(2.0 * pi * (float(leftX + i + dispatchThreadID.x) / g_samplesPerColorburstCycle + relativePhase), s, c);

    IQ += chroma * float2(c, -s);
  }

  IQ /= filterWidth;

  // Sample luminance at our current location, we don't need to do any filtering to it at all.
  float Y = g_sourceTexture.Load(uint3(dispatchThreadID, 0)).x;

  g_outputTexture[dispatchThreadID] = float4(Y, IQ, 1);
}