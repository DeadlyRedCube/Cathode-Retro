Texture2D<float4>  g_sourceTexture : register(t0);
RWTexture2D<uint> g_outputTexture : register(u0);


// This shader takes an RGB input texture and converts into a packed four-bit-per-pixel RGBI CGA image, by finding the
//  closest RGBI-generated color to the input color and using that as its output color.
static const float3 k_rgbColors[16] = 
{
  float3(0,        0,        0),          // black
  float3(0,        0,        0.666667),   // blue
  float3(0,        0.666667, 0),          // green
  float3(0,        0.666667, 0.666667),   // cyan
  float3(0.666667, 0,        0),          // red
  float3(0.666667, 0,        0.666667),   // magenta
  float3(0.666667, 0.333333, 0),          // brown
  float3(0.666667, 0.666667, 0.666667),   // light gray
  float3(0.333333, 0.333333, 0.333333),   // dark gray
  float3(0.333333, 0.333333, 0.666667),   // light blue
  float3(0.333333, 0.666667, 0.333333),   // light green
  float3(0.333333, 0.666667, 0.666667),   // light cyan
  float3(0.666667, 0.333333, 0.333333),   // light red
  float3(0.666667, 0.333333, 0.666667),   // light magenta
  float3(0.666667, 0.666667, 0.333333),   // light yellow
  float3(0.666667, 0.666667, 0.666667),   // white
};


float DistanceSqr(float3 a, float3 b)
{
  float3 d = b - a;
  return dot(d, d);
}


uint RGBIFromRGB(float3 rgb)
{
  float nearestDistance = DistanceSqr(rgb, k_rgbColors[0]);
  uint nearestIndex = 0;

  for (uint i = 1; i < 16; i++)
  {
    float d = DistanceSqr(rgb, k_rgbColors[i]);
    if (d < nearestDistance)
    {
      nearestDistance = d;
      nearestIndex = i;
    }
  }

  return nearestIndex;
}


[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  // Load two RGBs we're going to pack them
  float3 rgb1 = g_sourceTexture.Load(uint3(dispatchThreadID * uint2(2, 1), 0)).rgb;
  float3 rgb2 = g_sourceTexture.Load(uint3(dispatchThreadID * uint2(2, 1) + uint2(1, 0), 0)).rgb;

  uint rgbi1 = RGBIFromRGB(rgb1);
  uint rgbi2 = RGBIFromRGB(rgb2);

  g_outputTexture[dispatchThreadID] = (rgbi1 << 4) | rgbi2;
}