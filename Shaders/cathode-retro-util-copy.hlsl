///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Do a 1:1 copy of a texture

#include "cathode-retro-util-language-helpers.hlsli"
#include "cathode-retro-util-lanczos.hlsli"

DECLARE_TEXTURE2D(g_sourceTexture, g_sampler);

float4 Main(float2 inTexCoord)
{
  return SAMPLE_TEXTURE(g_sourceTexture, g_sampler, inTexCoord);
}

PS_MAIN;