///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader generates the Screen Texture, which is effectively overlayed on top of the actual screen image in the RGBToCRT shader.
//  This includes the emulation of the shadow mask (or slot mask, aperture grill) of a CRT screen, as well as masking around the edges
//  to handle blacking out values that are outside of the visible screen.


#include "cathode-retro-crt-distort-coordinates.hlsli"

// The shadow mask texture for the CRT. That is, if you think of an old CRT and how you could see the
//  R, G, and B dots, this is that texture. Needs to be set up to wrap, as well as to have mip mapping (and, ideally, anisotropic
//  filtering too, if rendering as a curved screen, to help with sharpness and aliasing).
DECLARE_TEXTURE2D(g_shadowMaskTexture, g_sampler);

// These are standard 8x and 16x sampling patterns (found in the D3D docs here:
//  https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels )
#if 1
  CONST int k_samplePointCount = 16;
  BEGIN_CONST_ARRAY(float2, k_samplingPattern, k_samplePointCount)
    float2( 1.0f / 8.0,  1.0f / 8.0),
    float2(-1.0f / 8.0, -3.0f / 8.0),
    float2(-3.0f / 8.0,  2.0f / 8.0),
    float2( 4.0f / 8.0, -1.0f / 8.0),
    float2(-5.0f / 8.0, -2.0f / 8.0),
    float2( 2.0f / 8.0,  5.0f / 8.0),
    float2( 5.0f / 8.0,  3.0f / 8.0),
    float2( 3.0f / 8.0, -5.0f / 8.0),
    float2(-2.0f / 8.0,  6.0f / 8.0),
    float2( 0.0f / 8.0, -7.0f / 8.0),
    float2(-4.0f / 8.0, -6.0f / 8.0),
    float2(-6.0f / 8.0,  4.0f / 8.0),
    float2(-8.0f / 8.0,  0.0f / 8.0),
    float2( 7.0f / 8.0, -4.0f / 8.0),
    float2( 6.0f / 8.0,  7.0f / 8.0),
    float2(-7.0f / 8.0, -8.0f / 8.0)
  END_CONST_ARRAY
#else
  CONST uint k_samplePointCount = 8;
  BEGIN_CONST_ARRAY(float2, k_samplingPattern, k_samplePointCount)
    float2( 1.0f / 8.0, -3.0f / 8.0),
    float2(-1.0f / 8.0,  3.0f / 8.0),
    float2( 5.0f / 8.0,  1.0f / 8.0),
    float2(-3.0f / 8.0, -5.0f / 8.0),
    float2(-5.0f / 8.0,  5.0f / 8.0),
    float2(-7.0f / 8.0, -1.0f / 8.0),
    float2( 3.0f / 8.0,  7.0f / 8.0),
    float2( 7.0f / 8.0, -7.0f / 8.0)
  END_CONST_ARRAY
#endif



CBUFFER consts
{
  // $NOTE: The first four values here are the same as the first four in RGBToCRT.hlsl, and are expected to match.

  // This shader is intended to render a screen of the correct shape regardless of the output render target shape, effectively letterboxing
  //  or pillarboxing as needed(i.e. rendering a 4:3 screen to a 16:9 render target). g_viewScale is the scale value necessary to get the
  //  resulting screen scale correct. In the event the output render target is wider than the intended screen, the screen needs to be
  //  scaled down horizontally to pillarbox, usually like (where screenAspectRatio is crtScreenWidth / crtScreenHeight):
  //    (x: (renderTargetWidth / renderTargetHeight) * (1.0 / screenAspectRatio), y: 1.0)
  //  and if the output render target is taller than the intended screen, it will end up letterboxed using something like:
  //    (x: 1.0, y: (renderTargetHeight / renderTargetWidth) * screenAspectRatio)
  // Note that if overscan (where the edges of the screen cover up some of the picture) is being emulated, it potentially needs to be
  //  taken into account in this value too. See RGBToCRT.h for details if that's the case.
  float2 g_viewScale;

  // If overscan emulation is intended (where the edges of the screen cover up some of the picture), then this is the amount of signal
  //  texture scaling needed to account for that. Given an overscan value "overscanAmount" that's
  //    (overscanLeft + overscanRight, overscanTop + overscanBottom)
  //  this value should end up being:
  //    (inputImageSize.xy - overscanAmount.xy) / inputImageSize.xy
  float2 g_overscanScale;

  // This is the texture coordinate offset to adjust for overscan. Because the input coordinates are [-1..1] instead of [0..1], this is
  //  the offset needed to recenter the value. Given an "overscanDifference" value:
  //    (overscanLeft - overscanRight, overscanTop - overscanBottom)
  //  this value should be:
  //    overscanDifference.xy/ inputImageSize.xy * 0.5
  float2 g_overscanOffset;

  // The amount along each axis to apply the virtual-curved screen distortion. Usually a value in [0..1]. "0" indicates no curvature
  //  (a flat screen) and "1" indicates "quite curved"
  float2 g_distortion;

  // The distortion amount used for where the edges of the screen mask should be (outside of which is just a solid dark color).
  //  Should be at least g_distortion, but if emulating an older TV which had some additional bevel curvature that cut off part of the
  //  picture, this can be applied by adding additional value to this.
  float2 g_maskDistortion;

  // The scale of the shadow mask texture. Higher value means the coordinates are scaled more and, thus, the shadow mask is smaller.
  float2 g_shadowMaskScale;

  // How much to round the corners of the screen to emulate a TV that had rounded corners that cut off a little picture.
  //  0 indicates no rounding (squared-off corners), while a value of 1.0 means "the whole screen is an oval." Values <= 0.2 are
  //  recommended.
  float  g_roundedCornerSize;
};



float4 Main(float2 inTexCoord)
{
  // First thing we want to do is scale our input texture coordinate to be in [-1..1] instead of [0..1]
  inTexCoord = inTexCoord * 2 - 1;

  // Calculate a separate set of distorted coordinates, this for the outer mask (which determines the masking off of extra-rounded screen
  //  edges).
  float2 maskT = DistortCRTCoordinates(inTexCoord * g_viewScale, g_maskDistortion);

  // Now distort our actual texture coordinates to get our texture into the correct space for display.
  float2 t = DistortCRTCoordinates(inTexCoord * g_viewScale, g_distortion) * g_overscanScale + g_overscanOffset * 2.0;

  // Calculate the signed distance to the edge of the "screen" taking the rounded corners into account. This will be used to generate the
  //  mask around the edges of the "screen" where we draw a dark color.
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

  // Use our signed distacce to edge of screen along with the ddx/ddy of our mask coordinate to generate a nicely-antialiased mask where
  //  0 means "fully outside of the screen" and 1 means "fully on-screen".
  float maskAlpha = 1.0 - smoothstep(g_roundedCornerSize - length(ddx(maskT) + ddy(maskT)), g_roundedCornerSize, edgeDist);

  // Now supersample the shadow mask texture with a mip-map bias to make it sharper. The supersampling will help counteract the bias and
  //  give us a sharp shadow mask with minimal-to-no aliasing.
  float2 dxT = ddx(t);
  float2 dyT = ddy(t);
  float3 color = float3(0, 0, 0);
  for (int i = 0; i < k_samplePointCount; i++)
  {
    color += SAMPLE_TEXTURE_BIAS(
      g_shadowMaskTexture,
      g_sampler,
      (t + k_samplingPattern[i].x * dxT + k_samplingPattern[i].y * dyT) * g_shadowMaskScale,
      -1).rgb;
  }

  color /= float(k_samplePointCount);

  // Our final texture contains the rgb value from the shadow mask, as well as the mask value in the alpha channel.
  //  Note that the color channel has not been premultiplied with the mask.
  return float4(color, maskAlpha);
}


PS_MAIN

