RWTexture2D<unorm float4> g_outputTexture : register(u0);

cbuffer consts : register(b0)
{
  float g_blackLevel;
  float g_coordinateScale;
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
  float2 t = float2(dispatchThreadID) * g_coordinateScale * float2(1.0, 2.0);

  if (t.x > 0.5)
  {
    t.x = t.x * 2.0 - 1.0;
    t.y = frac(t.y + 0.5);
  }
  else
  {
    t.x *= 2.0;
  }

  float3 color = float3(1, 0, 0);
  t.x *= 3;
  if (t.x >= 2)
  {
    color = float3(0,0,1);
  }
  else if (t.x >= 1)
  {
    color = float3(0,1,0);
  }
  t.x = frac(t.x);
  t.x /= 3;

  float2 border = float2(1.0/3.0 * (1.0 / 4.0), 1.0/6.0);
  float rounding = border.x / 3.0;
  border -= rounding;

  // Center each pixel
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