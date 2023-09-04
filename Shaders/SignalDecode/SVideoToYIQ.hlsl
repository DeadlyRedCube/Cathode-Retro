///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This signal takes an input S-Video signal and converts it into a set of YIQ color space values.
//
// This process is called NTSC color demodulation, which is a type of QAM demodulation.
//
// An NTSC signal (either composite or S-Video) is really just encoded YIQ data, where the Y value is encoded as luma and IQ is encoded
//  as a waveform in the chroma. Since this shader takes an effective S-Video signal as input, the Y component is easy - it's just the
//  luma channel of the input.
//
// Just like filtering the luma from the chroma (see the comment in CompositeToSVideo.hlsl), rather than doing any sort of fancy filtering
//  here, the absolute best results I've gotten were from a super-basic average (which very nicely removes a very specific frequency and
//  all integer multiples of it), which is also exactly what we need here.
//
// Additionally, if we want temporal artifact reduction, we're doing two of this calculation (we have a (lumaA, chromaA, lumaB, chromaB)
//  texture representing the same frame with two different color phases), and we use the temporal artifact reduction value to blend between
//  them, which reduces or eliminates alternating-phase-related flickering that systems like the NES could generate.
//
// The output of this shader is a texture that contains the decoded Y, I, and Q channels in R, G, and B  (plus 1.0 in alpha).


// This is a 2- or 4-component texture that contains either a single luma, chroma sample pair or two luma, chroma pairs of S-Video-like
//  signal. It's 2 components if we have no temporal artifact reduction (we're not blending two versions of the same frame), 4 if we do.
Texture2D<float4> g_sourceTexture : register(t0);

// This is a 1- or 2-component texture that contains the colorburst phase offsets for each scanline. It's 1 component if we have no
//  temporal artifact reduction, and 2 if we do.
// Each phase value in this texture is the phase in (fractional) multiples of the colorburst wavelength.
Texture2D<float2> g_scanlinePhases : register(t1);

// This sampler should be set up for linear filtering and clamped addressing (no wrapping).
sampler g_sampler : register(s0);

cbuffer consts : register(b0)
{
  // How many samples (horizontal texels) there are per each color wave cycle.
  uint g_samplesPerColorburstCycle;

  // A value representing the tint offset (in colorburst wavelengths) from baseline that we want to use. Mostly used for fun to emulate the
  //  tint dial of a CRT TV.
  float g_tint;

  // This is a value representing how saturated we want the output to be. 0 basically means we'll decode as a grayscale image, 1 means
  //  fully saturated color (i.e. the intended input saturation), and you could even set values greater than 1 to oversaturate.
  //  This corresponds to the saturation dial of a CRT TV.
  // $NOTE: This value should be pre-scaled by the g_brightness value, so that if brightness is 0, saturation is always 0 - otherwise,
  //  you get weird output values where there should have been nothing visible but instead you get a pure color instead.
  float g_saturation;

  // This is a value representing the brightness of the output. a value of 0 means we'll output pure black, and 1 means "the intended
  //  brightness based on the input signal". Values above 1 will over-brighten the output.
  //  This corresponds to the brightness dial of a CRT TV.
  float g_brightness;

  // This is the luma value of the input signal that represents black. For our synthetic signals it's typically 0.0, but from a real NTSC
  //  signal this can be some other voltage level, since a voltage of 0 typically indicates a horizontal or vertical blank instead.
  //  This is calculated from/generated with the composite or S-Video signal we were given.
  float g_blackLevel;

  // This is the luma value of the input signal that represents brightest white.  For our synthetic signals it's typically 1.0, but from
  //  a real NTSC signal (or if we've applied some signal artifacts like ghosting) it could be some other value.
  //  This is calculated from/generated with the composite or S-Video signal we were given.
  float g_whiteLevel;

  // A [0..1] value indicating how much we want to blend in an alternate version of the generated signal to adjust for any artifacting
  //  between successive frames. 0 means we only have (or want to use) a single input luma/chroma pair. A value > 0 means we are going to
  //  blend the results of two parallel-computed versions of our YIQ values, with a value of 1.0 being a pure average of the two.
  float g_temporalArtifactReduction;
}

static const float k_pi = 3.141592653;


float4 main(float2 inTexCoord : TEX): SV_TARGET
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y);

  // Get the index of our x sample.
  uint sampleXIndex = uint(round(inTexCoord.x / inputTexelSize.x - 0.5));

  // We're going to sample chroma at double our actual colorburst cycle to eliminate some deeply rough artifacting on the edges.
  //  In this case we're going to average by DOUBLE the color burst cycle - doing just a single cycle ends up with a LOT of edge
  //  artifacting on things, which are too strong. 2x is much softer but not so large that you can really notice that it's adding a ton of
  //  extra blurring on the color channel (it needs to be an integer multiple of our cycle sample count for the filtering to work).
  // $TODO: This could actually be reduced to 1 when we're doing an decode of a signal that was not originally composite, since there are
  //  no areas where luma changes will have crept into the color channel, which is typically the artifacting we see.
  uint filterWidth = g_samplesPerColorburstCycle * 2;

  float2 relativePhase = g_scanlinePhases.Sample(g_sampler, inTexCoord.y) + g_tint;

  // This is the chroma decode process, it's a QAM demodulation.
  //  You multiply the chroma signal by a reference waveform and its quadrature (Basically, sin and cos at a given time) and then filter
  //  out the chroma frequency (here done by a box filter (an average)). What you're left with are the approximate I and Q color space
  //  values for this part of the image.
  float4 centerSample = g_sourceTexture.Sample(g_sampler, inTexCoord);
  float2 Y = centerSample.xz;

  // $TODO: This could be made likely more efficient by basically doing two passes: one to generate a four-component texture with both sets
  //  of sin and cos-multiplied chroma values, and then THIS shader to average them, which would allow doing half the samples in this pass
  //  as we could take advantage of bilinear filtering.
  float4 IQ;
  {
    float2 s, c;
    sincos(2.0 * k_pi * (float(sampleXIndex) / g_samplesPerColorburstCycle + relativePhase), s, c);
    IQ = centerSample.yyww  * float4(s, -c).xzyw;
  }

  {
    int iterEnd = int((filterWidth - 1) / 2);
    for (int i = 1; i <= iterEnd; i++)
    {
      {
        float2 coord = inTexCoord + float2(i, 0) * inputTexelSize;
        float2 chroma = g_sourceTexture.Sample(g_sampler, coord).yw;
        float2 s, c;
        sincos(2.0 * k_pi * (float(sampleXIndex + i) / g_samplesPerColorburstCycle + relativePhase), s, c);
        IQ += chroma.xxyy  * float4(s, -c).xzyw;
      }

      {
        float2 coord = inTexCoord - float2(i, 0) * inputTexelSize;
        float2 chroma = g_sourceTexture.Sample(g_sampler, coord).yw;
        float2 s, c;
        sincos(2.0 * k_pi * (float(sampleXIndex - i) / g_samplesPerColorburstCycle + relativePhase), s, c);
        IQ += chroma.xxyy  * float4(s, -c).xzyw;
      }
    }

    if ((filterWidth & 1) == 0)
    {
      // We have an odd remainder (because we have an even filter width), so sample 0.5x each endpoint
      {
        float2 coord = inTexCoord + float2(iterEnd + 1, 0) * inputTexelSize;
        float2 chroma = g_sourceTexture.Sample(g_sampler, coord).yw;
        float2 s, c;
        sincos(2.0 * k_pi * (float(sampleXIndex + iterEnd + 1) / g_samplesPerColorburstCycle + relativePhase), s, c);
        IQ += 0.5 * chroma.xxyy  * float4(s, -c).xzyw;
      }

      {
        float2 coord = inTexCoord - float2(iterEnd + 1, 0) * inputTexelSize;
        float2 chroma = g_sourceTexture.Sample(g_sampler, coord).yw;
        float2 s, c;
        sincos(2.0 * k_pi * (float(sampleXIndex - iterEnd - 1) / g_samplesPerColorburstCycle + relativePhase), s, c);
        IQ += 0.5 * chroma.xxyy  * float4(s, -c).xzyw;
      }
    }

    IQ /= filterWidth;
  }

  // Adjust our components, first Y to account for the signal's black/white level (and user-chosen brightness), then IQ for saturation
  //  (Which should also include the signal's brightness scale)
  Y = (Y - g_blackLevel) / (g_whiteLevel - g_blackLevel) * g_brightness;
  IQ *= g_saturation.xxxx;

  // we have 1 or 2 components of Y, and 2 or 4 of IQ. Blend them together based on our temporal aliasing reduction to get our final
  //  decoded YIQ values.
  Y.x = lerp(Y.x, Y.y, g_temporalArtifactReduction * 0.5);
  IQ.xy = lerp(IQ.xy, IQ.zw, g_temporalArtifactReduction * 0.5);

  // Do some gamma adjustments (values effectively based on eyeballing the results of NTSC signals from NES, SNES, and Genesis consoles)
  Y.x = pow(saturate(Y.x), 2.0 / 2.2);
  float iqSat = saturate(length(IQ.xy));
  IQ.xy *= pow(iqSat, 2.0 / 2.2) / max(0.00001, iqSat);

  return float4(Y.x, IQ.xy, 1);
}