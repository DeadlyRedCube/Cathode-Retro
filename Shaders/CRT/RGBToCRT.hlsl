#include "DistortCRTCoordinates.hlsli"

sampler g_sampler : register(s0);

Texture2D<float4> g_currentFrameTexture: register(t0);
Texture2D<float4> g_previousFrameTexture : register(t1);
Texture2D<float4> g_screenMaskTexture : register(t2);
Texture2D<float4> g_diffusionTexture : register(t3);

cbuffer consts : register(b0)
{
  float2 g_viewScale;                   // Scale to get the correct aspect ratio of the image
  float2 g_overscanScale;               // overscanSize / standardSize;
  float2 g_overscanOffset;              // texture coordinate offset due to overscan
  float2 g_distortion;                  // How much to distort (from 0 .. 1)

  float  g_phosphorDecay;
  float  g_scanlineCount;               // How many scanlines there are
  float  g_scanlineStrength;            // How strong the scanlines are (0 == none, 1 == whoa)
  float  g_curEvenOddTexelOffset;       // This is 0.5 if it's an odd frame and -0.5 if it's even.
  float  g_prevEvenOddTexelOffset;      // This is 0.5 if it's an odd frame and -0.5 if it's even.
  float  g_diffusionStrength;           // This is a 0..1
  float  g_shadowMaskStrength;          // How much to blend the shadow mask in. 0 means "no shadow mask" and 1 means "fully apply"
}


static const float pi = 3.141592653;


float4 main(float2 inTexCoord : TEX) : SV_TARGET
{
  float4 screenMask = g_screenMaskTexture.Sample(g_sampler, inTexCoord);

  // Now distort our actual texture coordinates to get our texture into the correct space for display
  float2 t = DistortCRTCoordinates((inTexCoord * 2 - 1) * g_viewScale, g_distortion) * g_overscanScale + g_overscanOffset * 2.0;

  // Use "t" (before we do the even/odd update or the scanline-sharpening) to load our diffusion texture, which is an approximation of
  //  the glass in front of the phosphors scattering light a little bit due to imperfections.
  float3 diffusionColor = g_diffusionTexture.Sample(g_sampler, t * 0.5 + 0.5).rgb;

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

  // Sample the actual display texture and add in the previous frame (For phosphor decay)
  float3 sourceColor;
  {
    t = t * 0.5 + 0.5; // t has been in -1..1 range this whole time, scale it to 0..1 for sampling.
    sourceColor = g_currentFrameTexture.Sample(g_sampler, t).rgb;

    // Reduce the influence of the scanlines as we get small enough that aliasing is unavoidable (fully fading out at 0.7x nyquist)
    float scanlineStrength = lerp(g_scanlineStrength, 0, smoothstep(1.0, 1.4, length(ddy(inTexCoord)) * g_scanlineCount * 2));

    // It's possible we want to precalculate this like was done with the shadow mask overlay.
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
      //  got something that looked good at 1080p and up and introduced minimal moiré (minimal meaning "it's not visible when the shadow
      //  mask is also enabled).
      float scale = pow(abs(pixelLengthInScanlineSpace), 2.6) * 7;

      float ya = scanlineSpaceY - scale;
      float yb = scanlineSpaceY + scale;
      scanline = (0.5 * (yb - ya) + 1.0 / (2 * pi) * (sin(pi * ya) - sin(pi * yb))) / (2 * scale);

      // Now multiply in the scanline darkening according to the scanline strength.
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

    float3 prevSourceColor = g_previousFrameTexture.Sample(g_sampler, prevT).rgb;
    prevSourceColor *= lerp(1 - scanlineStrength, 1.0, prevScanline);

    // If a phosphor hasn't decayed all the way keep its brightness
    sourceColor = max(prevSourceColor * g_phosphorDecay, sourceColor);

    // We want to adjust the brightness to somewhat compensate for the darkening due to scanlines
    sourceColor *= 1.0 + scanlineStrength * 0.5;
  }

  // Time to put it all together: first, by applying the screen mask (i.e. the shadow mask/aperture grill, etc)...
  // $TODO: Figure out why this 0.15 is here - I'm sure it's some eyeballed "this looks good" value, but it would be nice to document
  //  why, specifically, if possible. Or maybe expose it as "Shadow map bias" or something.
  float3 res = sourceColor * ((screenMask.rgb - 0.15) * g_shadowMaskStrength + 1.0);

  // ... then bringing in some diffusion on top (This isn't physically accurate (it should really be a lerp between res and diffusionColor)
  //  but doing it this way preserves the brightness and still looks reasonable, especially when displaying bright things on a dark
  //  background)
  res = max(diffusionColor * g_diffusionStrength, res);

  // Finally, mask out everything outside of the edges
  return float4(lerp((0.05).xxx, res, screenMask.a), 1);
}
