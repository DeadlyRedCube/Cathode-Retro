Texture2D<unorm float4> g_sourceTexture : register(t0);
RWTexture2D<unorm float4> g_outputTexture : register(u0);

static const float k_lanczos2[8] = {-0.009, -0.042, 0.117, 0.434, 0.434, 0.117, -0.042, -0.009};


// Downsample an input image by 2x along each axis by using a lanczos filter.
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  uint2 dim;
  {
    g_sourceTexture.GetDimensions(dim.x, dim.y);
  }
  float4 color = float4(0.0, 0.0, 0.0, 0.0);
  for (int y = -3; y < 5; y++)
  {
    for (int x = -3; x < 5; x++)
    {
      uint2 coord = (dispatchThreadID * 2 + dim + uint2(x, y)) % dim;
      float4 samp = g_sourceTexture.Load(uint3(coord, 0));
      color += samp * k_lanczos2[x + 3] * k_lanczos2[y + 3];
    }
  }

  g_outputTexture[dispatchThreadID] = color;
}