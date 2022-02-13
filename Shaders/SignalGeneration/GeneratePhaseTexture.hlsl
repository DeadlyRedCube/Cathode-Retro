#include "../TrackingInstability.hlsli"

cbuffer consts : register (b0)
{
  float g_initialFrameStartPhase;
  float g_prevFrameStartPhase;
  float g_phaseIncrementPerScanline;
  uint g_samplesPerColorburstCycle;
  float g_instabilityScale;
  uint g_noiseSeed;
  uint g_signalTextureWidth;
  uint g_scanlineCount;
}


float4 main(float2 texCoord: TEX): SV_TARGET
{
  uint scanlineIndex = uint(round(texCoord.x * g_scanlineCount - 0.5));

  float instability = CalculateTrackingInstabilityOffset(
    scanlineIndex, 
    g_scanlineCount,
    g_noiseSeed,
    g_instabilityScale,
    g_signalTextureWidth);

  // Calculate our phase, offset by a half-signal texel to center the wave generated on the center of the texel (instead of it being the wave at the START of the texel, where it's not aligned with the
  //  sample data).
  float2 phases = 
    float2(g_initialFrameStartPhase, g_prevFrameStartPhase) 
    + g_phaseIncrementPerScanline * float(scanlineIndex) 
    + (0.5) / float(g_samplesPerColorburstCycle)
    + instability / float(g_samplesPerColorburstCycle) * g_signalTextureWidth;

  return float4(phases, 0, 0);
}