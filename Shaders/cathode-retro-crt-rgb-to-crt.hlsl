///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This shader combines the current frame, the previous frame, the screen mask, and the diffusion into the final render.
//  It's a relatively complex shader, and certainly if there are features that aren't needed for the current render it could be simplified.

// $TODO: May want to do something like an ubershader version that does exactly the minimum amount of work based on how many features
//  are actually in use (pulling out the distortion or, more importantly, the sampling of the previous/diffusion textures when they're
//  unneeded).


#include "cathode-retro-util-language-helpers.hlsli"
#include "cathode-retro-crt-distort-coordinates.hlsli"


// This is the RGB current frame texture - the output of the NTSC decode shaders if decoding was needed.
// This sampler should be set up with linear texture sampling and should be set to clamp the textures involved (no wrapping).
DECLARE_TEXTURE2D(g_currentFrameTexture, g_currentFrameSampler);

// This is the previous frame's texture (i.e. last frame's g_currentFrameTexture).
// This sampler should be set up with linear texture sampling and should be set to clamp the textures involved (no wrapping).
DECLARE_TEXTURE2D(g_previousFrameTexture, g_previousFrameSampler);

// This texture is the output of the GenerateScreenTexture shader, containing the (scaled, tiled, and antialiased) mask texture in
//  the rgb channels and the edge-of-screen mask value in the alpha channel. It is expected to have been generated at our output resolution
//  (i.e. it's 1:1 pixels with our output render target)
// This sampler should be set up with linear texture sampling and should be set to clamp the textures involved (no wrapping).
DECLARE_TEXTURE2D(g_screenMaskTexture, g_screenMaskSampler);

// This texture contains a tonemapped/blurred version of the input texture, to emulate the diffusion of the light from the phosphors
//  through the glass on the front of a CRT screen.
// This sampler should be set up with linear texture sampling and should be set to clamp the textures involved (no wrapping).
DECLARE_TEXTURE2D(g_diffusionTexture, g_diffusionSampler);


CBUFFER consts
{
  // $NOTE: The first four values here are the same as the first four in GenerateScreenTexture.hlsl, and are expected to match.

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

  // How much of the previous frame's brightness to keep. 0 means "we don't use the previous frame at all" and 1 means "the previous
  //  frame is at full brightness". In many CRTs, the phosphor persistence is short enough that it would be effectively 0 at 50-60fps
  //  (As a CRT's phospors could potentially be completely faded out by then). However, for some cases (for instance, interlaced video
  //  or for actual NES/SNES/probably other console output) it is generally preferable to turn on a little bit of persistance to lessen
  //  temporal flickering on an LCD screen as it can tend to look bad depending on the panel.
  float  g_phosphorPersistence;

  // How many scanlines there are in this field of the input (where a field is either the even or odd scanlines of an interlaced frame,
  //  or the entirety of a progressive-scan frame)
  float  g_scanlineCount;

  // The strength of the separation between scanlines. 0 means "no scanline separation at all" and 1 means "separate the scanlines as
  //  much as possible" - on high-enough resolution output render target (at 4k for sure) "1" means "fully black between scanlines", but to
  //  reduce aliasing that amount of separation will diminish at lower output resolution.
  float  g_scanlineStrength;

  // This is the scanline-space coordinate offset to use to adjust our texture coordinate's y value based on whether this is a (1-based)
  //  odd frame or an even frame. It will be 0.5 (shifting the texture up half a scanline) if it's an odd frame and -0.5 (shifting the
  //  texture down half a scanline) if it's an even frame.
  float  g_curEvenOddTexelOffset;

  // Same as above, but it's the even/odd texel offset that was relevant for the previous frame (so we can blend it in at the proper spot).
  //  This should match g_curEvenOddTexelOffset for a progressive-scan signal and should be "-g_curEvenOddTexelOffset" if interlaced.
  float  g_prevEvenOddTexelOffset;

  // This is how much diffusion to apply, blending in the diffusion texture which is an emulation of the light from the screen scattering
  //  in the glass on the front of the CRT - 0 means "no diffusion" and 1 means "a whole lot of diffusion".
  float  g_diffusionStrength;

  // How much we want to blend in the mask. 0 means "mask is not visible" and 1 means "mask is fully visible"
  float  g_maskStrength;
};


CONST float pi = 3.141592653;


float4 Main(float2 inTexCoord)
{
  // The screen texture is 1:1 with the output render target so sample it directly off of the input texture coordinates.
  float4 screenMask = SAMPLE_TEXTURE(g_screenMaskTexture, g_screenMaskSampler, inTexCoord);

  // Now distort the texture coordinates to get our texture into the correct space for display.
  float2 t = DistortCRTCoordinates((inTexCoord * 2 - 1) * g_viewScale, g_distortion) * g_overscanScale + g_overscanOffset * 2.0;

  // Use "t" (before we do the even/odd update or the scanline-sharpening) to load our diffusion texture, which is an approximation of
  //  the glass in front of the phosphors scattering light a little bit due to imperfections.
  float3 diffusionColor = SAMPLE_TEXTURE(g_diffusionTexture, g_diffusionSampler, t * 0.5 + 0.5).rgb;

  // Offset based on whether we're an even or odd frame
  t.y += g_curEvenOddTexelOffset / g_scanlineCount;

  // Before we adjust the y coordinate to sharpen the scanline interpolation, grab our scanline-space y coordinate.
  float scanlineSpaceY = t.y * g_scanlineCount + g_scanlineCount;

  // Because t.y is currently in [-1, 1], this derivative multiplied by the scanline count ends up being the number of total scanlines
  //  involved (including the empty ones). So this is "how much along y does one output pixel move us relative to g_scanlineCount*2"
  float pixelLengthInScanlineSpace = length(ddy(t)) * g_scanlineCount;

  // Do a little magic to sharpen up the interpolation between scanlines - a CRT (didn't really have any vertical smoothing, so we want to
  //  make the centers of our texels a little more solid and do less bilinear blending vertically (just a little to simulate the softness
  //  of the screen in general)
  {
    float scanlineIndex = (t.y * 0.5 + 0.5) * g_scanlineCount;
    float scanlineFrac = frac(scanlineIndex);
    scanlineIndex -= scanlineFrac;
    scanlineFrac -= 0.5;
    float signFrac = sign(scanlineFrac);
    float ySharpening = 0.1; // Any value from [0, 0.5) should work here, larger means the vertical pixels are sharper
    scanlineFrac = sign(scanlineFrac) * saturate(abs(scanlineFrac) - ySharpening) * 0.5 / (0.5 - ySharpening);

    scanlineIndex += scanlineFrac + 0.5;
    t.y = float(scanlineIndex) / g_scanlineCount * 2 - 1;
  }

  // Sample the actual display texture and add in the previous frame (For phosphor persistence)
  float3 sourceColor;
  {
    t = t * 0.5 + 0.5; // t has been in -1..1 range this whole time, scale it to 0..1 for sampling.
    sourceColor = SAMPLE_TEXTURE(g_currentFrameTexture, g_currentFrameSampler, t).rgb;

    // Reduce the influence of the scanlines as we get small enough that aliasing is unavoidable (fully fading out at 0.7x nyquist - early
    //  to ensure that we don't introduce any aliasing as we get too close).
    float scanlineStrength = lerp(g_scanlineStrength, 0, smoothstep(1.0, 1.4, length(ddy(inTexCoord)) * g_scanlineCount * 2));

    // $TODO: We may want to find a way to precalculate this scanline value as a texture (like with the screen texture). Unfortunately
    //  the screen texture is already using all 4 of its components so we'd need a new one, which is why I didn't).
    float scanline;
    {
      // For the actual scanline value, we use the following equation:
      //   cos(scanlineSpaceY * pi) * 0.5 + 0.5
      //  That is, at scanline centers it's either 0 or 1. However, to avoid moiré patterns we actually want to supersample it.
      //  The good news is, we can supersample over a range using numeric integration since it's a sinusoid. The integration of that wave
      //  between y coordinates ya and yb ends up being:
      //   (yb - ya)/2 + 1/(2pi) * (sin(pi*ya) - sin(pi*yb)))
      //  but in order to turn it into an average we need to divide that result by the width of the range, which is (yb - ya).

      // As pixelLengthInScanlineSpace gets larger (i.e. effective output resolution gets smaller) we want to ramp up the blurring
      //  dramatically to avoid moiré effects. There's no real mathematical basis for this algorithm, I just eyeballed a curve until I
      //  got something that looked good at 1080p and up and introduced minimal moiré (minimal meaning "it's not visible when the mask is
      //  also enabled").
      float scale = pow(abs(pixelLengthInScanlineSpace), 2.6) * 7;

      float ya = scanlineSpaceY - scale;
      float yb = scanlineSpaceY + scale;
      scanline = (0.5 * (yb - ya) + 1.0 / (2 * pi) * (sin(pi * ya) - sin(pi * yb))) / (2 * scale);

      // Now multiply in the scanline-spacing darkening according to the scanline strength.
      sourceColor *= lerp(1 - scanlineStrength, 1.0, scanline);
    }

    float2 prevT = t;
    float prevScanline = scanline;
    if (g_prevEvenOddTexelOffset != g_curEvenOddTexelOffset)
    {
      // We have a different scanline parity in the previous frame so we need to offset our texture coordinate (to put the prev frame's
      //  scanline center at the correct spot) and then invert our scanline multiplier (to darken the alternate scanlines)
      prevT.y += g_prevEvenOddTexelOffset / g_scanlineCount;
      prevScanline = 1 - prevScanline;
    }

    // Sample the previous texture and darken the area between scanlines accordingly.
    float3 prevSourceColor = SAMPLE_TEXTURE(g_previousFrameTexture, g_previousFrameSampler, prevT).rgb;
    prevSourceColor *= lerp(1 - scanlineStrength, 1.0, prevScanline);

    // Blend our previous frame into the current one based on how much phosphor persistence we have between frames.
    sourceColor = max(prevSourceColor * g_phosphorPersistence, sourceColor);

    // We want to adjust the brightness to somewhat compensate for the darkening due to scanlines
    sourceColor /= 1.0 - scanlineStrength * 0.5;
  }

  // Time to put it all together: first, by applying the screen mask (i.e. the shadow mask/aperture grill, etc)...
  // $TODO: Figure out why this 0.15 is here - I'm sure it's some eyeballed "this looks good" value, but it would be nice to document
  //  why, specifically, if possible. Or maybe expose it as "mask bias" or something.
  float3 res = sourceColor * ((screenMask.rgb - 0.15) * g_maskStrength + 1.0);

  // ... then bringing in some diffusion on top (This isn't physically accurate (it should really be a lerp between res and diffusionColor)
  //  but doing it this way preserves the brightness and still looks reasonable, especially when displaying bright things on a dark
  //  background)
  res = max(diffusionColor * g_diffusionStrength, res);

  // Finally, mask out everything outside of the edges to get our final output value.
  float3 gray = float3(0.05, 0.05, 0.05);
  return float4(lerp(gray, res, screenMask.a), 1);
}


PS_MAIN