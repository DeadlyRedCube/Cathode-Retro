///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader applies ghosting and noise to an SVideo or Composite signal.
//
// Noise is just that, some randomized signal offset per sample to simulate analog error in the signal.
//
// Ghosting basically is what happens when a copy of your signal "skips" its intended path through the cable and mixes in with your
//  normal signal (like an EM leak of the signal) and is basically a pre-echo of the signal. So as a very rough approximation of this,
//  we'll just blend in an offset, blurred, and scaled version of the original signal.


#include "cathode-retro-util-language-helpers.hlsli"
#include "cathode-retro-util-noise.hlsli"


// The texture to apply artifacts to. Should either be:
//  - a 1-channel composite signal
//  - a 2-channel luma/chroma S-Video signal
//  - a 2-channel doubled composite texture (two composite versions of the same frame for temporal artifact reduction)
//  - a 4-channel luma/chroma/luma/chroma doubled S-Video signal (two S-Video versions of the same frame for temporal artifact reduction)
// This sampler should be set to linear sampling, and either clamp addressing or perhaps border mode, depending.
DECLARE_TEXTURE2D(g_sourceTexture, g_sampler);


CBUFFER consts
{
  // This represents how much ghosting is visible - 0.0 is "no ghosting" and 1.0 is "the ghost is as strong as the original signal."
  float g_ghostVisibility;

  // This is the distance (in colorburst cycles) before the original signal that ghosting should appear.
  float g_ghostDistance;

  // How far to blur the ghost, in colorburst cycles.
  float g_ghostSpreadScale;


  // The strength of noise to add to the signal - 0.0 means no noise, 1.0 means, just, a lot of noise. So much. Probably too much.
  float g_noiseStrength;

  // An integer seed used to generate the per-output-texel noise. This can be a monotonically-increasing value, or random every frame,
  //  just as long as it's different every frame so that the noise changes.
  uint g_noiseSeed;


  // The width of the input signal texture
  uint g_signalTextureWidth;

  // The number of scanlines in this field of NTSC video
  uint g_scanlineCount;

  // How many samples (texels along a scanline) there are per colorburst cycle (the color wave in the composite signal).
  uint g_samplesPerColorburstCycle;
};


float4 Main(float2 inputTexCoord)
{
  float4 signal = SAMPLE_TEXTURE(g_sourceTexture, g_sampler, inputTexCoord);
  if (g_ghostVisibility != 0)
  {
    // Calculate the center sample position of our ghost based on our input params, as well as how far to spread our sampling for the
    //  blur.
    float2 ghostCenterCoord = inputTexCoord
      + float2(g_ghostDistance * g_samplesPerColorburstCycle, 0) / float2(g_signalTextureWidth, g_scanlineCount);
    float2 ghostSampleSpread = float2(g_ghostSpreadScale * g_samplesPerColorburstCycle, 0)
      / float2(g_signalTextureWidth, g_scanlineCount);

    // The following is a 9-tap gaussian, written as 5 samples using bilinear interpolation to sample two taps at once:
    // 0.00761441700, 0.0360749699, 0.109586075, 0.213444546, 0.266559988, 0.213444546, 0.109586075, 0.0360749699, 0.00761441700
    float4 ghost = float4(0, 0, 0, 0);
    ghost += SAMPLE_TEXTURE(g_sourceTexture, g_sampler, ghostCenterCoord - ghostSampleSpread * 1.174285279339) * 0.0436893869;
    ghost += SAMPLE_TEXTURE(g_sourceTexture, g_sampler, ghostCenterCoord - ghostSampleSpread * 1.339243613069) * 0.323030611;
    ghost += SAMPLE_TEXTURE(g_sourceTexture, g_sampler, ghostCenterCoord)                                      * 0.266559988;
    ghost += SAMPLE_TEXTURE(g_sourceTexture, g_sampler, ghostCenterCoord + ghostSampleSpread * 1.339243613069) * 0.323030611;
    ghost += SAMPLE_TEXTURE(g_sourceTexture, g_sampler, ghostCenterCoord + ghostSampleSpread * 1.174285279339) * 0.0436893869;

    signal += ghost * g_ghostVisibility;
  }

  // Also add some noise for each texel. Use the noise seed and our x/y position to c
  uint2 pixelIndex = uint2(floor(inputTexCoord * float2(g_signalTextureWidth, g_scanlineCount)));
  uint localNoiseSeed = (g_noiseSeed * g_scanlineCount + pixelIndex.y) * g_signalTextureWidth + pixelIndex.x / 2U;

  // To make it a little more analogue-seeming, generate 3 instances of the noise to act as a 3-tap blur.
  float noise = WangHashAndXorShift(localNoiseSeed)
    + 0.5 * (WangHashAndXorShift(localNoiseSeed + 1U) + WangHashAndXorShift(localNoiseSeed - 1U));

  // Finally we'll scale and bias the noise and return it added to our signal.
  return signal + (noise - 1) * g_noiseStrength * 0.5;
}

PS_MAIN