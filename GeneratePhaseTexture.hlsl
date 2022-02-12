RWTexture2D<float2> g_outputTexture : register(u0);

cbuffer consts : register (b0)
{
  float g_initialFrameStartPhase;
  float g_prevFrameStartPhase;
  float g_phaseIncrementPerScanline;
  uint g_samplesPerColorburstCycle;
}


[numthreads(8, 1, 1)]
void main(uint dispatchThreadID : SV_DispatchThreadID)
{
  // Calculate our phase, offset by a half-signal texel to center the wave generated on the center of the texel (instead of it being the wave at the START of the texel, where it's not aligned with the
  //  sample data).
  g_outputTexture[uint2(dispatchThreadID, 0)] = float2(g_initialFrameStartPhase, g_prevFrameStartPhase) + g_phaseIncrementPerScanline * float(dispatchThreadID) + 0.5 / float(g_samplesPerColorburstCycle);
}