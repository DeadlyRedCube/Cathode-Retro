///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader does a horizontal three-tap blur or sharpen to each input scanline.


#include "cathode-retro-util-language-helpers.hlsli"


// The input RGB texture that will be filtered. It should be set up for linear filtering and clamp addressing.
DECLARE_TEXTURE2D(g_sourceTexture, g_sampler);

CBUFFER consts
{
  // This is the strength of the blur - 0.0 will leave the output texture unchanged from the input, 1.0 will do a full
  //  3-texel average, and -1 will do a very extreme sharpen pass.
  float g_blurStrength;

  // The step between each sample, in texels. Usually scaled to be relative to the original input, which may have had a
  //  different resolution than our input texture due to the NTSC generation/decode process (if there's no original
  //  input, i.e. we're not using the NTSC signal generator and have gotten a signal straight from a real NTSC signal,
  //  then you'd just want to pick some nice-on- average value instead)
  float g_stepSize;
};


float4 Main(float2 inTexCoord)
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y);

  // Just a simple three-tap 1D filter.
  // $TODO: This could absolutely be done with two samples instead of 3, by taking advantage of linear filtering the
  //  same way we do in the gaussian/lanczos filters.
  float blurSide = g_blurStrength / 3.0;
  float blurCenter = 1.0 - 2.0 * blurSide;
  float4 color = SAMPLE_TEXTURE(g_sourceTexture, g_sampler, (inTexCoord - float2(g_stepSize, 0) * inputTexelSize))
                 * blurSide
               + SAMPLE_TEXTURE(g_sourceTexture, g_sampler, (inTexCoord))
                 * blurCenter
               + SAMPLE_TEXTURE(g_sourceTexture, g_sampler, (inTexCoord + float2(g_stepSize, 0) * inputTexelSize))
                 * blurSide;

  return color;
}


PS_MAIN