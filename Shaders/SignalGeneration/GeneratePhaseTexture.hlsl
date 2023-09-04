///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader generates the phase of the colorburst for each scanline.
//
// Basically, outside of the visible portion of a color NTSC scanline, there is a colorburst which the TV uses to tell which color is the
//  reference phase (which is yellow in color).  We're not generating a full NTSC signal, but we still need to generate this data, and to
//  do that we have a set of emulated timings for the virtual system that is generating our output.


#include "../TrackingInstability.hlsli"


cbuffer consts : register (b0)
{
  // This is the colorburst phase (in fractional multiples of the colorburst wavelength) for the first scanline in our generated signal.
  float g_initialFrameStartPhase;

  // This is the colorburst phase for the first scanline of the previous frame - this is used if we're generating two sets of phase
  //  information for use with temporal artifact reduction.
  float g_prevFrameStartPhase;

  // The amount that the phase increments every scanline, in (fractional) multiples of the colorburst wavelength.
  float g_phaseIncrementPerScanline;

  // How many samples (texels along a scanline) there are per colorburst cycle (the color wave in the composite signal).
  uint g_samplesPerColorburstCycle;

  // The scale of any picture instability (horizontal scanline-by-scanline tracking issues). This is used to ensure the phase values
  //  generated line up with the offset of the texture sampling that happens in RGBToSVideoOrComposite. Must match the similarly-named
  //  value in RGBToSVideoOrComposite.
  float g_instabilityScale;

  // A seed for the noise used to generate the scanline-by-scanline picture instability. Must match the simiarly-named value in
  //  RGBToSVideoOrComposite.
  uint g_noiseSeed;

  // The width of the signal texture that is being generated, in texels.
  uint g_signalTextureWidth;

  // The number of scanlines for this field of video.
  uint g_scanlineCount;
}


float4 main(float2 texCoord: TEX): SV_TARGET
{
  uint scanlineIndex = uint(round(texCoord.x * g_scanlineCount - 0.5));

  // Calculate the phase for the start of the given scanline.
  float2 phases = float2(g_initialFrameStartPhase, g_prevFrameStartPhase) + g_phaseIncrementPerScanline * float(scanlineIndex);

  // Offset by half a cycle so that it lines up with the *center* of the filter instead of the left edge of it.
  phases += (0.5) / float(g_samplesPerColorburstCycle);

  // Finally, offset by the scaled instability so that if there are color artifacts, they'll have the correct phase.
  // $TODO: This is actually slightly wrong, and I'm assuming it's due to the way the texture sampling works when generating the signal
  //  texture.  This value is slightly too high when we're at half a colorburst phase offset (by something like 0.05, found by manually
  //  tweaking the tint slider to get a close match), and I'm not entirely sure why. Intuition says that because we're not oversampling
  //  the RGB texture to generate the signal texture, any intentional linear-blending texture offsets (like for 1bpp CGA modes) aren't
  //  actually offset *exactly* halfway at that point, and so the calculated phase on NTSC demodulation is slightly off from true.
  float instability = CalculateTrackingInstabilityOffset(
    scanlineIndex,
    g_scanlineCount,
    g_noiseSeed,
    g_instabilityScale,
    g_signalTextureWidth);
  phases += instability * float(g_signalTextureWidth) / float(g_samplesPerColorburstCycle);

  return float4(frac(phases), 0, 0);
}