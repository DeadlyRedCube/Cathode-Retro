Texture2D<float4> g_sourceTexture : register(t0);
Texture2D<float> g_scanlinePhases : register(t1);
RWTexture2D<float2> g_outputTexture : register(u0);

cbuffer consts : register (b0)
{
  uint g_outputTexelsPerColorburstCycle;

  uint g_inputWidth;
  uint g_outputWidth;
}


static const float pi = 3.1415926535897932384626433832795028841971f;

[numthreads(8, 8, 1)]
void main(int2 dispatchThreadID : SV_DispatchThreadID)
{
  // $TODO Sample, don't load, for things where it doesn't divide nicely into the output size 
  //  (sampling has to be done carefully - ideally you want 
  float3 RGB = g_sourceTexture.Load(uint3(dispatchThreadID * float2(float(g_inputWidth) / float(g_outputWidth), 1), 0)).rgb; 

  float3x3 mat = float3x3(
    0.1100, -0.3217,  0.3121,  // r
    0.5900, -0.2773, -0.5251,  // g
    0.3000,  0.5990,  0.2130); // b

  // Convert RGB to YIQ
  float3 YIQ = mul(RGB, mat);

  // Separate them out because YIQ.y ended up being confusing (is it y? no it's i! but also it's y!)
  float Y = YIQ.x;
  float I = YIQ.y;
  float Q = YIQ.z;

  // Figure out where in the carrier wave we are
  float scanlinePhase = g_scanlinePhases.Load(uint3(dispatchThreadID.y, 0, 0)).x;
  float phase = scanlinePhase + dispatchThreadID.x / float(g_outputTexelsPerColorburstCycle);

  // Now we need to encode our IQ component in the carrier wave at the correct phase
  float s, c;
  sincos(2.0 * pi * phase, s, c);

  float luma = Y;
  float chroma = -s * Q + c * I;

  g_outputTexture[dispatchThreadID] = float2(luma, chroma * 2.0);
}