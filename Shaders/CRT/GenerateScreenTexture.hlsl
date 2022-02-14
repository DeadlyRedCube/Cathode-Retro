#include "DistortCRTCoordinates.hlsli"

Texture2D<float4> g_shadowMaskTexture : register(t0);

sampler g_sampler : register(s0);


cbuffer consts : register(b0)
{
  float2 g_viewScale;           // Scale to get the correct aspect ratio of the image
  float2 g_overscanScale;       // overscanSize / standardSize;
  float2 g_overscanOffset;      // texture coordinate offset due to overscan
  float2 g_distortion;          // How much to distort (from 0 .. 1)
  float2 g_maskDistortion;      // Where to put the mask sides

  float2 g_shadowMaskScale;     // Scale of the shadow mask texture lookup
  float  g_shadowMaskStrength;  // 
  float  g_roundedCornerSize;   // 0 == no corner, 1 == screen is an oval
  float  g_phosphorDecay;
  float  g_scanlineCount;       // How many scanlines there are
  float  g_scanlineStrength;    // How strong the scanlines are (0 == none, 1 == whoa)

  float  g_signalTextureWidth;
}


float4 main(float2 inTexCoord : TEX) : SV_TARGET
{
  inTexCoord = inTexCoord * 2 - 1;

  // Calculate a separate set of distorted coordinates, this for the outer mask (which determines the masking off of extra-rounded screen edges)
  float2 maskT = DistortCRTCoordinates(inTexCoord * g_viewScale, g_maskDistortion);

  // Now distort our actual texture coordinates to get our texture into the correct space for display
  float2 shadowMaskT = DistortCRTCoordinates(inTexCoord * g_viewScale, g_distortion) * g_overscanScale + g_overscanOffset * 2.0;
  
  // Calculate the signed distance to the edge of the "screen" taking the rounded corners into account.
  float edgeDist;
  {
    float2 upperQuadrantT = abs(maskT);
    float innerRoundArea = 1.0 - g_roundedCornerSize;
    if (upperQuadrantT.x < innerRoundArea || upperQuadrantT.y < innerRoundArea)
    {
      // We're not in the rounded corner's space, so use the longest axis' value as our edge position.
      edgeDist = max(upperQuadrantT.x, upperQuadrantT.y) - innerRoundArea;
    }
    else
    {
      // Calculate the distance to the rounded corner.
      edgeDist = length(saturate(upperQuadrantT - innerRoundArea));
    }
  }

  // Masking value blends nicely in based on our signed distance and the mask's rough length.
  float maskAlpha = 1.0 - smoothstep(g_roundedCornerSize - length(ddx(maskT) + ddy(maskT)), g_roundedCornerSize, edgeDist);

  // Calculate the scanline multiplier
  // $TODO Do some moire-reduction magic
  // $TODO Also when we finally add interlacing back in, the scanline info will need to move to a separate one-component texture
  float scanlineMultiplier;
  {
    static const float k_pi = 3.141597;
    scanlineMultiplier = sqrt(sin(((shadowMaskT.y + 0.125f) * g_scanlineCount) * 2.0 * k_pi) * 0.5 + 0.5);
    scanlineMultiplier = (scanlineMultiplier * g_scanlineStrength) + 1.0 - g_scanlineStrength * 0.60;
  }
  
  // Sample the shadow mask
  // $TODO Do some moire-reduction magic
  float3 shadowMaskColor = g_shadowMaskTexture.SampleBias(
    g_sampler, 
    shadowMaskT * g_shadowMaskScale,
    0.25).rgb;

  shadowMaskColor = (shadowMaskColor - 0.15) * g_shadowMaskStrength + 1.0;
  return float4((scanlineMultiplier * shadowMaskColor), maskAlpha);
}
