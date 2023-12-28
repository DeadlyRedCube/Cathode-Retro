///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader takes an input S-Video signal and modulates the chroma channel with a reference waveform (relative to
//  the per-scanline phase) in preparation for getting the I and Q chroma channels (of the YIQ color space) for
//  conversion to RGB. This is essentially a pre-pass for cathode-retro-decoder-svideo-to-rgb.


#include "cathode-retro-util-language-helpers.hlsli"


// This is a 2- or 4-component texture that contains either a single luma, chroma sample pair or two luma, chroma pairs
//  of S-Video-like signal. It is 2 components if we have no temporal artifact reduction (we're not blending two
//  versions of the same frame), 4 if we do.
// This sampler should be set up for linear filtering and clamped addressing (no wrapping).
DECLARE_TEXTURE2D(g_sourceTexture, g_sourceSampler);

// This is a 1- or 2-component texture that contains the colorburst phase offsets for each scanline. It's 1 component
//  if we have no temporal artifact reduction, and 2 if we do.
// Each phase value in this texture is the phase in (fractional) multiples of the colorburst wavelength.
// This sampler should be set up for linear filtering and clamped addressing (no wrapping).
DECLARE_TEXTURE2D(g_scanlinePhases, g_scanlinePhasesSampler);


CBUFFER consts
{
  // How many samples (horizontal texels) there are per each color wave cycle.
  uint g_samplesPerColorburstCycle;

  // A value representing the tint offset (in colorburst wavelengths) from baseline that we want to use. Mostly used
  //  for fun to emulate the tint dial of a CRT TV.
  float g_tint;

  // The width of the input signal
  uint g_inputWidth;
};


CONST float k_pi = 3.141592653;


float4 Main(float2 inTexCoord)
{
  uint sampleXIndex = uint(floor(inTexCoord.x * g_inputWidth));

  // Get the reference phase for our scanline
  float2 relativePhase = SAMPLE_TEXTURE(g_scanlinePhases, g_scanlinePhasesSampler, inTexCoord.yy).xy + g_tint;

  // This is the chroma decode process, it's a QAM demodulation.
  //  You multiply the chroma signal by a reference waveform and its quadrature (Basically, sin and cos at a given
  //  time) and then filter out the chroma frequency (here done by a box filter (an average)). What you're left with
  //  are the approximate I and Q color space values for this part of the image.
  float4 chroma = SAMPLE_TEXTURE(g_sourceTexture, g_sourceSampler, inTexCoord).yyww;

  // This is part of the chroma decode process, separated out into its own shader for efficiency's sake - it's the
  //  first half of a QAM demodulation. Each piece of the chroma signal is multiplied by a reference waveform and its
  //  quadrature (basically, sin and cos at a given time) and then filter out the chroma frequency (done in the next
  //  pass).
  float2 s, c;
  sincos(2.0 * k_pi * (float(sampleXIndex) / g_samplesPerColorburstCycle + relativePhase), s, c);

  // Return the chromas modulated with our sines and cosines (swizzled to line them up correctly)
  return chroma * float4(s, -c).xzyw;
}


PS_MAIN