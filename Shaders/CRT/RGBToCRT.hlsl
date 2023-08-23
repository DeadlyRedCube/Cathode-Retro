#include "DistortCRTCoordinates.hlsli"

sampler g_sampler : register(s0);

Texture2D<float4> g_currentFrameTexture: register(t0);
Texture2D<float4> g_previousFrameTexture : register(t1);
Texture2D<float4> g_screenMaskTexture : register(t2);

cbuffer consts : register(b0)
{
  float2 g_viewScale;           // Scale to get the correct aspect ratio of the image
  float2 g_overscanScale;       // overscanSize / standardSize;
  float2 g_overscanOffset;      // texture coordinate offset due to overscan
  float2 g_distortion;          // How much to distort (from 0 .. 1)
  float2 g_maskDistortionUnused; 

  float2 g_shadowMaskScaleUnused;
  float  g_shadowMaskStrengthUnused;
  float  g_roundedCornerSizeUnused;
  float  g_phosphorDecay;
  float  g_scanlineCount;       // How many scanlines there are
  float  g_scanlineStrength;    // How strong the scanlines are (0 == none, 1 == whoa)
}

float4 main(float2 inTexCoord : TEX) : SV_TARGET
{
  float4 screenMask = g_screenMaskTexture.Sample(g_sampler, inTexCoord);
  
  // Now distort our actual texture coordinates to get our texture into the correct space for display
  float2 t = DistortCRTCoordinates((inTexCoord * 2 - 1) * g_viewScale, g_distortion) * g_overscanScale + g_overscanOffset * 2.0;

  // Do a little magic to sharpen up the interpolation between scanlines - a CRT (didn't really have any vertical smoothing, so we want to
  //  make the centers of our texels a little more solid and do less bilinear blending vertically (just a little to simulate the softness of
  //  the screen in general)
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
    
    // We want to adjust the brightness to somewhat compensate for the darkening due to scanlines
    sourceColor *= 1.0 + g_scanlineStrength * 0.5;
    
    float3 prevSourceColor = g_previousFrameTexture.Sample(g_sampler, t).rgb;

    // If a phosphor hasn't decayed all the way keep its brightness
    sourceColor = max(prevSourceColor * g_phosphorDecay, sourceColor);
  }

  // Put it all together
  return float4(lerp((0.05).xxx, sourceColor * screenMask.rgb, screenMask.a), 1);
}
