///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader takes a composite signal (like you'd get via a console's composite input) and separates out the luma from the chroma so it
//  is effectively an SVideo signal. I tried all sorts of wacky hijinks to filter these things out in a solid emulation of what old CRT
//  hardware was doing, but all of those filters were both VERY expensive and also had annoying artifacts (like ringing). I tried all sorts
//  of low pass and band stop solutions to get the luma separated from the chroma.
//
// However, it turns out, that all you REALLY need to do (if your color cycle frequency is a nice multiple of your pixel resolution, which
//  it is), is average out that many pixels' worth of signal, and that flattens out the wave frequency very nicely, leaving you with just
//  the luma. This average (or box filter) is very effective at removing exactly the frequency that we care about and leaving the rest.
//  And it's about the simplest (and highest-performance) possible way to do it.
//
//  Basically, if you average every sample of the below sine wave, what you end up with is a value that represents the y coordinate of its
//   center (the line in the diagram).
//         _....._
//      ,="       "=.
//    ,"             ".
//   /                 ".
//   ---------------------.-------------------/
//                         ".               ."
//                           "._         _,"
//                              "-.....-"
//
//
// That center line corresponds to the "luma" portion of the composite signal - the effective brightness of that portion of the scanline
//  (or, if you prefer, the Y component in the YIQ encoding).
//
// Once you have that, you can simply subtract this luma value from the original signal and you're left with JUST the wave information
//  (which is the chroma, or IQ portion of the YIQ encoding).
//
// It's also worth noting that this shader is capable of doing TWO luma/chroma demodulations at once (one to the r and g output channels
//  and one to b and a), for purposes of reducing/eliminating any jitter between two frames for signals where the color phases alternate
//  frame by frame (the NES and SNES do this, for instance).


#include "cathode-retro-util-language-helpers.hlsli"
#include "cathode-retro-util-box-filter.hlsli"


// This texture is expected to represent the visible signal part of an NTSC field (a full frame for a progressive scan input, or one half
//  of an interlaced frame for an interlaced one). If this were using the input from a true NTSC signal, this would only contain
//  information in the red channel - however, some emulated systems (the NES and SNES, for instance) have different chroma phase components
//  on alternating frames, and we can pass a variant of the current frame with a different phase to average and reduce the flickering of
//  temporal artifacts from frame to frame ... this doesn't make much difference if your emulator is running at a perfectly smooth 60fps,
//  but it can make a big difference if there are frame drops).
// The sampler should be set to linear sampling, and clamped addressing (no wrapping).
DECLARE_TEXTURE2D(g_sourceTexture, g_sampler);

CBUFFER consts
{
  // How many samples (texels along a scanline) there are per colorburst cycle (the color wave in the composite signal). This shader
  //  currently assumes that it's integral (hence the int input) but it is totally possible to make the shader support a floating-point
  //  value instead - it would just need to do an extra fractional addition of the last texel.
  uint g_samplesPerColorburstCycle;
};


float4 Main(float2 inTex)
{
  float2 inputTexDim;
  GET_TEXTURE_SIZE(g_sourceTexture, inputTexDim);

  // Doing a box filter (an average) at exactly the colorburst cycle length is a *very* effective and efficient way to remove the chroma
  //  information completely, leaving with just the luma.
  float4 centerSample;
  float2 luma = BoxFilter(
    PASS_TEXTURE2D_AND_SAMPLER_PARAM(g_sourceTexture, g_sampler),
    1.0 / inputTexDim,
    g_samplesPerColorburstCycle,
    inTex,
    centerSample).xy;

  // Take our one or two computed luma channels and output them (swizzled) as r and b, then subtract those one or two two lumas and
  //  subtract them from the corresponding input waves to get the separated chroma component, output as g and a.
  // This way, if our render target is just a two-component texture (we're not doing any temporal aliasing reduction), then r will be the
  //   luma and g will be the chroma.
  return float4(luma, centerSample.xy - luma).rbga;
}


PS_MAIN