RWTexture2D<unorm float4> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  float g_blackLevel;
  float g_coordinateScale;
}

// Generate a texture that approximates the pattern of a CRT shadow mask (At least, one variety of one). We're going to generate
//  two sets of RGB rounded rectangles, where the right-most set is offset 50% vertically (so it wraps around)
[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  float2 t = float2(dispatchThreadID) * g_coordinateScale * float2(1.0, 2.0);

  if (t.x > 0.5)
  {
    // We're in the right-most half of the texture, so offset our y coordinate by 0.5 (and wrap it to within 0..1) so this set draws 
    //  offset vertically
    t.y = frac(t.y + 0.5);

    // Scale x to be within 0..1
    t.x = t.x * 2.0 - 1.0;
  }
  else
  {
    // Scale x to be within 0..1
    t.x *= 2.0;
  }

  // Figure out which of the three "zones" we're in - R, G, or B

  // Default to "red zone"
  float3 color = float3(1, 0, 0);
  t.x *= 3;
  if (t.x >= 2)
  {
    // blue zone
    color = float3(0,0,1);
  }
  else if (t.x >= 1)
  {
    // green zone
    color = float3(0,1,0);
  }

  // Convert our coordinate to be a 0 to 1/3rd value
  t.x = frac(t.x);
  t.x /= 3;

  float2 border = float2(1.0/3.0 * (1.0 / 4.0), 1.0/6.0);
  float rounding = border.x / 3.0;
  border -= rounding;

  // Center the rectangle so we can do a distance calcualtion from center
  t -= float2(1.0 / 6.0, 0.5);
  t = abs(t);
  t -= (float2(1.0 / 6.0, 0.5)) - (rounding + border);
  t /= rounding;
  t = max(0.0, t);

  float distance = length(t);
  float delta = 1.0 * g_coordinateScale / rounding;
  float mul = saturate(1.0 - smoothstep(1.0 - delta * 0.5, 1.0 + delta * 0.5, distance));

  color *= mul;

  color = color / (1.0 - g_blackLevel) + g_blackLevel;
  
  g_outputTexture[dispatchThreadID] = float4(color, 1);
}