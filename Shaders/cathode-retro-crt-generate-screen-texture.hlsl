///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader generates the Screen Texture, which is effectively overlayed on top of the actual screen image in the
//  RGBToCRT shader. This includes the emulation of the shadow mask (or slot mask, aperture grill) of a CRT screen, as
//  well as masking around the edges to handle blacking out values that are outside of the visible screen.


#include "cathode-retro-crt-distort-coordinates.hlsli"
#include "cathode-retro-util-noise.hlsli"

// The mask texture for the CRT. That is, if you think of an old CRT and how you could see the
//  R, G, and B dots, this is that texture. Needs to be set up to wrap, as well as to have mip mapping (and, ideally,
//  anisotropic filtering too, if rendering as a curved screen, to help with sharpness and aliasing).
DECLARE_TEXTURE2D(g_maskTexture, g_sampler);

// This is a 64-tap poisson disc filter, found here:
//  https://www.geeks3d.com/20100628/3d-programming-ready-to-use-64-sample-poisson-disc/
CONST int k_samplePointCount = 64;
BEGIN_CONST_ARRAY(float2, k_samplingPattern, k_samplePointCount)
  float2(-0.613392, 0.617481),
  float2(0.170019, -0.040254),
  float2(-0.299417, 0.791925),
  float2(0.645680, 0.493210),
  float2(-0.651784, 0.717887),
  float2(0.421003, 0.027070),
  float2(-0.817194, -0.271096),
  float2(-0.705374, -0.668203),
  float2(0.977050, -0.108615),
  float2(0.063326, 0.142369),
  float2(0.203528, 0.214331),
  float2(-0.667531, 0.326090),
  float2(-0.098422, -0.295755),
  float2(-0.885922, 0.215369),
  float2(0.566637, 0.605213),
  float2(0.039766, -0.396100),
  float2(0.751946, 0.453352),
  float2(0.078707, -0.715323),
  float2(-0.075838, -0.529344),
  float2(0.724479, -0.580798),
  float2(0.222999, -0.215125),
  float2(-0.467574, -0.405438),
  float2(-0.248268, -0.814753),
  float2(0.354411, -0.887570),
  float2(0.175817, 0.382366),
  float2(0.487472, -0.063082),
  float2(-0.084078, 0.898312),
  float2(0.488876, -0.783441),
  float2(0.470016, 0.217933),
  float2(-0.696890, -0.549791),
  float2(-0.149693, 0.605762),
  float2(0.034211, 0.979980),
  float2(0.503098, -0.308878),
  float2(-0.016205, -0.872921),
  float2(0.385784, -0.393902),
  float2(-0.146886, -0.859249),
  float2(0.643361, 0.164098),
  float2(0.634388, -0.049471),
  float2(-0.688894, 0.007843),
  float2(0.464034, -0.188818),
  float2(-0.440840, 0.137486),
  float2(0.364483, 0.511704),
  float2(0.034028, 0.325968),
  float2(0.099094, -0.308023),
  float2(0.693960, -0.366253),
  float2(0.678884, -0.204688),
  float2(0.001801, 0.780328),
  float2(0.145177, -0.898984),
  float2(0.062655, -0.611866),
  float2(0.315226, -0.604297),
  float2(-0.780145, 0.486251),
  float2(-0.371868, 0.882138),
  float2(0.200476, 0.494430),
  float2(-0.494552, -0.711051),
  float2(0.612476, 0.705252),
  float2(-0.578845, -0.768792),
  float2(-0.772454, -0.090976),
  float2(0.504440, 0.372295),
  float2(0.155736, 0.065157),
  float2(0.391522, 0.849605),
  float2(-0.620106, -0.328104),
  float2(0.789239, -0.419965),
  float2(-0.545396, 0.538133),
  float2(-0.178564, -0.596057)
END_CONST_ARRAY



CBUFFER consts
{
  // $NOTE: The first four values here are the same as the first four in RGBToCRT.hlsl, and are expected to match.

  // This shader is intended to render a screen of the correct shape regardless of the output render target shape,
  //  effectively letterboxing or pillarboxing as needed(i.e. rendering a 4:3 screen to a 16:9 render target).
  //  g_viewScale is the scale value necessary to get the resulting screen scale correct. In the event the output
  //  render target is wider than the intended screen, the screen needs to be scaled down horizontally to pillarbox,
  //  usually like (where screenAspectRatio is crtScreenWidth / crtScreenHeight):
  //    (x: (renderTargetWidth / renderTargetHeight) * (1.0 / screenAspectRatio), y: 1.0)
  //  if the output render target is taller than the intended screen, it will end up letterboxed using something like:
  //    (x: 1.0, y: (renderTargetHeight / renderTargetWidth) * screenAspectRatio)
  // Note that if overscan (where the edges of the screen cover up some of the picture) is being emulated, it
  //  potentially needs to be taken into account in this value too. See RGBToCRT.h for details if that's the case.
  float2 g_viewScale;

  // If overscan emulation is intended (where the edges of the screen cover up some of the picture), then this is the
  //  amount of signal texture scaling needed to account for that. Given an overscan value "overscanAmount" that's
  //    (overscanLeft + overscanRight, overscanTop + overscanBottom)
  //  this value should end up being:
  //    (inputImageSize.xy - overscanAmount.xy) / inputImageSize.xy
  float2 g_overscanScale;

  // This is the texture coordinate offset to adjust for overscan. Because the input coordinates are [-1..1] instead
  //  of [0..1], this is the offset needed to recenter the value. Given an "overscanDifference" value:
  //    (overscanLeft - overscanRight, overscanTop - overscanBottom)
  //  this value should be:
  //    overscanDifference.xy/ inputImageSize.xy * 0.5
  float2 g_overscanOffset;

  // The amount along each axis to apply the virtual-curved screen distortion. Usually a value in [0..1]. "0" indicates
  //  no curvature (a flat screen) and "1" indicates "quite curved"
  float2 g_distortion;

  // The distortion amount used for where the edges of the screen mask should be. Should be at least g_distortion, but
  //  if emulating an older TV which had some additional bevel curvature that cut off part of the picture, this can be
  //  applied by adding additional value to this.
  float2 g_maskDistortion;

  // The scale of the mask texture. Higher value means the coordinates are scaled more and, thus, the shadow mask is
  //  smaller.
  float2 g_maskScale;

  // The aspect ratio of the source texture (width / height, used for aspect correction)
  float g_aspect;

  // How much to round the corners of the screen to emulate a TV with rounded corners that cut off a little picture.
  //  0 indicates no rounding (squared-off corners), while a value of 1.0 means "the whole screen is an oval."
  //  Values <= 0.2 are recommended.
  float  g_roundedCornerSize;

};



float4 Main(float2 inTexCoord)
{
  uint2 pixelCoord8k = uint2(round(inTexCoord * float2(7680, 4320)));

  // First thing we want to do is scale our input texture coordinate to be in [-1..1] instead of [0..1] and adjust for
  float2 scaledTexCoord = (inTexCoord * 2 - 1) * g_viewScale;

  // Distort these coordinates to get the -1..1 screen area:
  float2 t = DistortCRTCoordinates(scaledTexCoord, g_distortion);

  // Calculate a separate set of distorted coordinates, this for the outer mask (which determines the masking off of
  //  extra-rounded screen edges).
  float2 maskT = DistortCRTCoordinates(t, g_maskDistortion.yx);

  // Adjust for overscan
  t = t * g_overscanScale + g_overscanOffset * 2.0;

  // Calculate the signed distance to the edge of the "screen" taking the rounded corners into account. This will be
  //  used to generate the mask around the edges of the "screen" where we draw a dark color.
  float edgeDist;
  {
    // Get our coordinate as just an "upper quadrant" coordinate, scaled to have the correct display aspect ratio.
    float2 sq = float2(g_aspect, 1.0) / max(1.0, g_aspect);
    float2 upperQuadrantT = abs(maskT);

    // Get the signed distance to a rounded rectangle to get our corners.
    float2 q = upperQuadrantT * sq - sq + g_roundedCornerSize;
    edgeDist = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - g_roundedCornerSize;
  }

  // Use our signed distance to edge of screen along with the ddx/ddy of our mask coordinate to generate a nicely-
  //  antialiased mask where 0 means "fully outside of the screen" and 1 means "fully on-screen".
  float maskAlpha = 1.0 - smoothstep(-length(ddx(maskT) + ddy(maskT)), 0.0, edgeDist);
  if (max(abs(scaledTexCoord.x), abs(scaledTexCoord.y)) > 1.1)
  {
    maskAlpha = 0.0;
  }

  // Now supersample the mask texture with a mip-map bias to make it sharper. The supersampling will help counteract
  //  the bias and give us a sharp mask with minimal-to-no aliasing.
  float angle = Noise2D(t * 1000, 10.0) * 6.28318531;
  float2 rotX = float2(sin(angle), cos(angle)) * 1.414;
  float2 rotY = float2(-rotX.y, rotX.x);

  float2 dxT = float2(dot(rotX, ddx(t)), dot(rotY, ddx(t)));
  float2 dyT = float2(dot(rotX, ddy(t)), dot(rotY, ddy(t)));
  float3 color = float3(0, 0, 0);
  for (int i = 0; i < k_samplePointCount; i++)
  {
    color += SAMPLE_TEXTURE_BIAS(
      g_maskTexture,
      g_sampler,
      (t + k_samplingPattern[i].x * dxT + k_samplingPattern[i].y * dyT) * g_maskScale,
      -2).rgb;
  }

  color /= float(k_samplePointCount);

  // Our final texture contains the rgb value from the mask, as well as the mask value in the alpha channel.
  //  Note that the color channel has not been premultiplied with the mask.
  return float4(color, maskAlpha);
}


PS_MAIN

