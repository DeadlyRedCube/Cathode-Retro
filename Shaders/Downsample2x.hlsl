// Downsample an input image by 2x along each axis by using a lanczos filter.


// This is a lanczos2 kernel for a nice 2x downsample. I wish I'd documented how I generated it but I didn't so instead this is what you
//  get. Sorry bout that.
//static const float k_lanczos2[8] = {-0.009, -0.042, 0.117, 0.434, 0.434, 0.117, -0.042, -0.009};

// However, it's been optimized to take advantage of the linear filtering using the same technique I used for the gaussian filters at
//  https://drilian.com/gaussian-kernel/
// That means it's only 4 texture samples for an 8-tap lanczos instead of a full 8, meaning this dumb 8x8 filter is 16 taps instead of 
//  64. Obviously this could be done in two 4-tap passes (horizontal then vertical) but this is done as a preprocess at the moment so
//  I didn't bother.
static const uint k_sampleCount = 4;
static const float k_coeffs[4] = 
{
  -0.051,
   0.551,
   0.551,
  -0.051,
};

static const float k_offsets[4] = 
{
  -2.67647052,
  -0.712341249,
   0.712341189,
   2.67647052,
};


Texture2D<unorm float4> g_sourceTexture : register(t0);
sampler g_sampler : register(s0);


float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y) * 0.5;
  inTexCoord -= 0.5 * inputTexelSize;

  float4 color = float4(0,0,0,0);
  for (uint y = 0; y < k_sampleCount; y++)
  {
    for (uint x = 0; x < k_sampleCount; x++)
    {
      float4 samp = g_sourceTexture.Sample(g_sampler, inTexCoord + float2(k_offsets[x], k_offsets[y]) * inputTexelSize);
      color += samp * k_coeffs[x] * k_coeffs[y];
    }
  }

  return color;
}