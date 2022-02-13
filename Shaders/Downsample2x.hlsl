Texture2D<unorm float4> g_sourceTexture : register(t0);

static const float k_lanczos2[8] = {-0.009, -0.042, 0.117, 0.434, 0.434, 0.117, -0.042, -0.009};

sampler g_sampler : register(s0);


// Downsample an input image by 2x along each axis by using a lanczos filter.
// $TODO This could be done in half the taps (and also separably) to be more efficient, but since it's only done as a texture-generation process, I
//  kinda don't care.
float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y) * 0.5;
  inTexCoord -= 0.5 * inputTexelSize;

  uint2 dim;
  {
    g_sourceTexture.GetDimensions(dim.x, dim.y);
  }
  float4 color = float4(0.0, 0.0, 0.0, 0.0);
  for (int y = -3; y < 5; y++)
  {
    for (int x = -3; x < 5; x++)
    {
      float4 samp = g_sourceTexture.SampleLevel(g_sampler, inTexCoord + float2(x, y) * inputTexelSize, 0);
      
      color += samp * k_lanczos2[x + 3] * k_lanczos2[y + 3];
    }
  }

  return color;
}