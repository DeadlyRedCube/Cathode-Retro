struct PSInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEX;
};


sampler SamDefault : register(s0);
sampler SamWrap : register(s1);

Texture2D<float4> Tex : register(t0);
Texture2D<float4> PrevTex : register(t1);
Texture2D<float4> ShadowMaskTex : register(t2);

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

// Do a barrel distortion (and horizontal noise wobble) to a given texture coordinate
float2 DistortCoordinates(float2 tex, float2 distortion, float noiseOffset)
{
  static const float k_viewAspect = 16.0 / 9.0;
  static const float k_piOver4 = 0.78539816339;
  static const float k_hFOV = 1.0471975512; // 60 degrees

  float z = cos(tex.x * distortion.x * k_piOver4) * cos(tex.y * distortion.y * k_piOver4);

  float maxY = 1.0 / cos(distortion.y * k_piOver4);
  float maxX = 1.0 / cos(distortion.x * k_piOver4);

  tex.x += noiseOffset;
  tex /= z;
  tex /= float2(maxX, maxY);
  return tex;
}


float4 main(PSInput input) : SV_TARGET
{
  // Calculate a separate set of distorted coordinates, this for the outer mask (which determines the masking off of extra-rounded screen edges)
  float2 maskT = DistortCoordinates(input.tex * g_viewScale, g_maskDistortion, 0);

  // Now distort our actual texture coordinates to get our texture into the correct space for display
  float2 t =           DistortCoordinates(input.tex * g_viewScale, g_distortion, 0) * g_overscanScale + g_overscanOffset * 2.0;
  
  // Take that calculation as our initial shadowMask texture (Before we start sharpening the vertical resolution of the sampling and the like)
  float2 shadowMaskT = t;

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
  float mul = 1.0 - smoothstep(g_roundedCornerSize - length(ddx(maskT) + ddy(maskT)), g_roundedCornerSize, edgeDist);

  // Sample the actual display texture with the appropriate blurring.
  float4 sourceColor;
  {
    t = t * 0.5 + 0.5; // t has been in -1..1 range this whole time, scale it to 0..1 for sampling.

    sourceColor = Tex.Sample(SamDefault, t);
    
    float4 prevSourceColor = PrevTex.Sample(SamDefault, t);

    // If a phosphor hasn't decayed all the way keep its brightness
    sourceColor = max(prevSourceColor * g_phosphorDecay, sourceColor);
  }
  
  // Calculate the scanline multiplier
  float scanlineMultiplier;
  {
    static const float k_pi = 3.141597;
    scanlineMultiplier = sqrt(sin(((shadowMaskT.y + 0.125f) * g_scanlineCount) * 2.0 * k_pi) * 0.5 + 0.5);
    scanlineMultiplier = (scanlineMultiplier * g_scanlineStrength) + 1.0 - g_scanlineStrength * 0.60;
  }
  
  // Put it all together
  float4 shadowMaskColor = ((ShadowMaskTex.Sample(SamWrap, shadowMaskT * g_shadowMaskScale) - 0.15) * g_shadowMaskStrength + 1.0);
  return lerp((0.05).xxxx, sourceColor * scanlineMultiplier * shadowMaskColor, mul);
}
