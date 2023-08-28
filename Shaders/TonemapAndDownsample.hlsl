// Downsample an input image by 2x along each axis by using a lanczos filter.

// $TODO: Make the Downsample2x filter separable and move the Lanczos2x into its own header.

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


cbuffer consts : register(b0)
{
  float2 g_downsampleDir;
  float g_minLuminosity;

  float g_colorPower;
}


float4 main(float2 inTexCoord: TEX): SV_TARGET
{
  float2 inputTexelSize = float2(ddx(inTexCoord).x, ddy(inTexCoord).y) * 0.5;
  // inTexCoord.y -= 0.5 * inputTexelSize;

  float4 v = float4(0, 0, 0, 0);
  for (uint i = 0; i < k_sampleCount; i++)
  {
    float2 c = inTexCoord + g_downsampleDir * inputTexelSize * k_offsets[i];
    
    float4 samp = g_sourceTexture.Sample(g_sampler, c);
    
    float inLuma = dot(samp.rgb, float3(0.3, 0.59, 0.11));
    
    float outLuma = saturate(inLuma - g_minLuminosity) / (1.0 - g_minLuminosity);
    outLuma = pow(outLuma, g_colorPower);
    
    samp.rgb *= outLuma / inLuma;
    
    v += samp * k_coeffs[i];
  }

  return v;
}