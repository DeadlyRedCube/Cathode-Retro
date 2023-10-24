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

  // The darkest output value in the shadow mask. 0 means the bits inbetween the R, G, and B values will be black.
  float g_blackLevel;
};


float4 Main(float2 inTexCoord)
{
  uint2 texelIndex = uint2(inTexCoord * g_texSize - 0.5);
  float2 t = float2(texelIndex) / g_texSize.x * float2(1.0, 2.0);

  // Adjust our texture coordinates based on whether we're in the left half of the output or the right half. In each we'll end up with
  //  a x, y in [0..1] square that will contain the R, G, B rectangles.
  if (t.x > 0.5)
  {
    // We're in the right-most half of the texture, so offset our y coordinate by 0.5 (and wrap it to within 0..1) so this set draws
    //  offset vertically.
    t.y = frac(t.y + 0.5);

    // Scale x to be within 0..1 where 0 is the middle of our texture (and the left edge of this rightmost square) and 1 is on the right
    //  edge of both.
    t.x = t.x * 2.0 - 1.0;
  }
  else
  {
    // We are in the left half of our texture, so scale the x coordinate so that it is 1.0 at the middle of the texture (so in this
    //  leftmost square it goes from [0..1]). The y coordinate is unchanged.
    t.x *= 2.0;
  }

  // Figure out which of the three "zones" we're in - R, G, or B - i.e. which horizontal third of the square we are in.
  // We'll start by multiplying by 3 so that x in [0..1) is R, [1..2) is G, and [2..3] is B.
  t.x *= 3;

  // Default to "red zone"
  float3 color = float3(1, 0, 0);
  if (t.x >= 2)
  {
    // blue zone
    color = float3(0,0,1);
  }
  else if (t.x >= 1)
  {
    // green zone
    color = float3(0,1,0);
  }

  // Convert our coordinate to be in [0..1/3rd]. Note that it was already multiplied by 3 above so this "frac" call gets us into the
  //  [0..1] value for this third.
  t.x = frac(t.x);
  t.x /= 3;

  // These are the positions of where the border of the colored rectangles will go in our third, as well as how round the edges will be.
  //  $NOTE: These are technically constants (and should compile down to such), but border isn't written as such because
  float2 border = float2(1.0/3.0 * (1.0 / 4.0), 1.0/6.0);
  float rounding = border.x / 3.0;
  border -= rounding;

  // Center the rectangle so we can do a distance calculation from center to find the bounds of the rectangle, using that to make a final
  //  [0..1] multiply value to scale our chosen rectangle color down to black out the areas outside of the colored portion.
  t -= float2(1.0 / 6.0, 0.5);
  t = abs(t);
  t -= (float2(1.0 / 6.0, 0.5)) - (rounding + border);
  t /= rounding;
  t = max(float2(0.0, 0.0), t);

  float distance = length(t);
  float delta = 1.0 / (g_texSize.x * rounding);
  float mul = saturate(1.0 - smoothstep(1.0 - delta * 0.5, 1.0 + delta * 0.5, distance));

  color *= mul;

  // Rescale the color so that instead of going from [0..1] it goes from [blackLevel..1]
  color = color / (1.0 - g_blackLevel) + g_blackLevel;

  return float4(color, 1);
}


PS_MAIN