///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader generates an approximation of a CRT slot mask (the little R, G, and B dots that you can see on some CRTs when you are close
//  to them.
// It is intended to be rendered to a texture that has a 2:1 width:height ratio, and we are generating two sets of RGB rounded rectangles,
//  where the right-most set is offset 50% vertically (so it wraps from bottom to top).


#include "cathode-retro-util-language-helpers.hlsli"

CBUFFER consts
{
  // The size of the render target we are rendering to.
  //  NOTE: It is expected that width == 2 * height.
  float2 g_texSize;
};


float4 Main(float2 inTexCoord)
{
  uint texelIndex = uint(inTexCoord.x * g_texSize.x - 0.5);
  float x = float(texelIndex) / g_texSize.x;

  x = frac(x * 2) * 3;

  // Default to "red zone"
  float3 color = float3(1, 0, 0);
  if (x >= 2)
  {
    // blue zone
    color = float3(0,0,1);
  }
  else if (x >= 1)
  {
    // green zone
    color = float3(0,1,0);
  }

  // Convert our coordinate to be in [0..1/3rd]. Note that it was already multiplied by 3 above so this "frac" call gets us into the
  //  [0..1] value for this third.
  x = frac(x);
  x /= 3;

  // These are the positions of where the border of the colored rectangles will go in our third, as well as how round the edges will be.
  //  $NOTE: These are technically constants (and should compile down to such), but border isn't written as such because
  float border = 1.0 / 12.0;
  float smoothing = border / 3.0;
  border -= smoothing;

  // Do similar calculations to the slot mask texture generation, except without any y component since there are no breaks along the
  //  vertical axis in an aperture grille. This could likely be simplified, but I already had that logic and this is fine for something
  //  that is intended to run once ever (at startup).
  x = abs(x - 1.0 / 6.0);
  x -= 1.0 / 6.0 - (smoothing + border);
  x /= smoothing;
  x = max(0.0, x);

  float delta = 1.0 / (g_texSize.x * smoothing);
  float mul = saturate(1.0 - smoothstep(1.0 - delta * 0.5, 1.0 + delta * 0.5, x));

  color *= mul;
  return float4(color, 1);
}


PS_MAIN