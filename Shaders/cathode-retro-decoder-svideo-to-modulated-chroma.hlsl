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


#include "cathode-retro-util-language-helpers.hlsli"


// This is a 2- or 4-component texture that contains either a single luma, chroma sample pair or two luma, chroma pairs of S-Video-like
//  signal. It's 2 components if we have no temporal artifact reduction (we're not blending two versions of the same frame), 4 if we do.
// This sampler should be set up for linear filtering and clamped addressing (no wrapping).
DECLARE_TEXTURE2D(g_sourceTexture, g_sourceSampler);

// This is a 1- or 2-component texture that contains the colorburst phase offsets for each scanline. It's 1 component if we have no
//  temporal artifact reduction, and 2 if we do.
// Each phase value in this texture is the phase in (fractional) multiples of the colorburst wavelength.
// This sampler should be set up for linear filtering and clamped addressing (no wrapping).
DECLARE_TEXTURE2D(g_scanlinePhases, g_scanlinePhasesSampler);


CBUFFER consts
{
  // How many samples (horizontal texels) there are per each color wave cycle.
  uint g_samplesPerColorburstCycle;

  // A value representing the tint offset (in colorburst wavelengths) from baseline that we want to use. Mostly used for fun to emulate the
  //  tint dial of a CRT TV.
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
  //  You multiply the chroma signal by a reference waveform and its quadrature (Basically, sin and cos at a given time) and then filter
  //  out the chroma frequency (here done by a box filter (an average)). What you're left with are the approximate I and Q color space
  //  values for this part of the image.
  float4 chroma = SAMPLE_TEXTURE(g_sourceTexture, g_sourceSampler, inTexCoord).yyww;

  // This is part of the chroma decode process, separated out into its own shader for efficiency's sake - it's the first half of a QAM
  //  demodulation. Each piece of the chroma signal is multiplied by a reference waveform and its quadrature (basically, sin and cos at a
  //  given time) and then filter out the chroma frequency (done in the next pass).
  float2 s, c;
  sincos(2.0 * k_pi * (float(sampleXIndex) / g_samplesPerColorburstCycle + relativePhase), s, c);

  // Return the chromas modulated with our sines and cosines (swizzled to line them up correctly)
  return chroma * float4(s, -c).xzyw;
}


PS_MAIN