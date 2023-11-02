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
  float2 hexGridF = inTexCoord * float2(6.0, 4.0);

  bool isOddBlock = (frac(hexGridF.y * 0.5) >= 0.5);
  if (isOddBlock)
  {
    hexGridF.x += .5;
  }

  int2 hexID = int2(floor(hexGridF));

  float2 cellCoord = frac(hexGridF);

  const float t = 0.5773502691; // tan(30 degrees)
  const float c = 0.5 * t;

  if (cellCoord.y < (-t * cellCoord.x) + c)
  {
    hexID.y--;
    hexGridF.x += isOddBlock ? -0.5 : 0.5;
    hexID.x -= int(isOddBlock);
  }
  else if (cellCoord.y < (t * cellCoord.x) - c)
  {
    hexID.y--;
    hexGridF.x += isOddBlock ? -0.5 : 0.5;
    hexID.x += int(!isOddBlock);
  }

  if (frac(float(hexID.y) *0.5) >= 0.5)
  {
    hexGridF.x ++;
    hexID.x ++;
  }

  float3 color;

  uint hx = uint(hexID.x + 1);
  if ((hx % 3U) == 0U)
  {
    color = float3(1, 0, 0);
  }
  else if ((hx % 3U) == 1U)
  {
    color = float3(0, 1, 0);
  }
  else
  {
    color = float3(0, 0, 1);
  }

  cellCoord = ((hexGridF - float2(hexID)) * float2(1.0, 1.0 / (1.0 + c)));
  cellCoord = cellCoord * 2 - 1;

  float dist = length(cellCoord);
  dist = 1.0 - smoothstep(0.85, 0.9, dist);

  // color *= lerp(0.1, 1.0, float(hexID.y % 2));
  return float4(color * dist, 1);
}


PS_MAIN