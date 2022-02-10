Texture2D<uint> g_sourceTexture : register(t0);
Texture2D<float2> g_scanlinePhases : register(t1);
RWTexture2D<float4> g_outputTexture : register(u0);

cbuffer consts : register (b0)
{
  uint g_outputTexelsPerColorburstCycle;

  uint g_inputWidthInPixels;
  uint g_outputWidth;

  uint g_useNewCGA;

  float g_compositeBlend;
}


// $TODO This is not great - I thought I could bypass the whole thing by just using measured CGA values from
//  some sources, but it didn't actually work out how I wanted, so I'll have to find another way.
static const float3 k_yiqOldCGA[16] =
{
  float3(0.002039216, -0.000174118, 0.003283137),
  float3(0.346901953, -0.211161569, 0.236002341),
  float3(0.372196108, -0.050147854, -0.250313342),
  float3(0.351960778, -0.234245121, -0.208735287),
  float3(0.336039245, 0.229198813, 0.213630974),
  float3(0.333725512, -0.018890202, 0.311062753),
  float3(0.373333335, 0.117631361, -0.142149031),
  float3(0.726039231, -0.001826674, -0.010684699),
  float3(0.271921575, 0.002000779, 0.007401563),
  float3(0.592941225, -0.127898067, 0.155223519),
  float3(0.627176464, 0.012756467, -0.308226287),
  float3(0.625725448, -0.237855688, -0.208346680),
  float3(0.610549033, 0.229198813, 0.213630974),
  float3(0.590548992, 0.032834083, 0.260881960),
  float3(0.617647052, 0.205941170, -0.227823511),
  float3(0.995921552, 0.000348210, -0.006566271),
};


static const float3 k_yiqNewCGA[16] =
{
  float3(0.000000000, 0.000000000, 0.000000000),
  float3(0.070588231, -0.113803923, 0.160235286),
  float3(0.307372540, -0.025484711, -0.196385100),
  float3(0.326666653, -0.168866679, -0.110223532),
  float3(0.155372560, 0.163994491, 0.111836076),
  float3(0.175529420, 0.018089399, 0.200445488),
  float3(0.419843107, 0.107105106, -0.169307441),
  float3(0.628862739, -0.004349828, -0.008236870),
  float3(0.250274509, 0.002174899, 0.004118446),
  float3(0.445215702, -0.108192563, 0.167248219),
  float3(0.686901927, -0.022961587, -0.198832959),
  float3(0.707058787, -0.168866694, -0.110223532),
  float3(0.534313738, 0.162558794, 0.116343141),
  float3(0.555176497, 0.014478832, 0.200834095),
  float3(0.801411748, 0.109454066, -0.168472156),
  float3(1.000000000, 0.000000000, 0.000000000),
};

static const float pi = 3.1415926535897932384626433832795028841971f;

[numthreads(8, 8, 1)]
void main(int2 dispatchThreadID : SV_DispatchThreadID)
{
  uint rgbiPacked = g_sourceTexture.Load(
    uint3(
      (dispatchThreadID / uint2(2, 1))
      * float2(float(g_inputWidthInPixels) / float(g_outputWidth), 1), 
    0)); 

  uint rgbi = (dispatchThreadID.x & 1) ? (rgbiPacked & 0x0F) : (rgbiPacked >> 4);

  float3 YIQ = g_useNewCGA ? k_yiqNewCGA[rgbi] : k_yiqOldCGA[rgbi];

  // Separate them out because YIQ.y ended up being confusing (is it y? no it's i! but also it's y!)
  float Y = YIQ.x;
  float I = YIQ.y;
  float Q = YIQ.z;

  // Figure out where in the carrier wave we are
  float2 scanlinePhase = g_scanlinePhases.Load(uint3(dispatchThreadID.y, 0, 0));

  // Calculate the phase, and then offset by half a texel to account for the off-center nature of our IQ demodulate filter.
  float2 phase = scanlinePhase + dispatchThreadID.x / float(g_outputTexelsPerColorburstCycle) + 0.5 / float(g_outputTexelsPerColorburstCycle);

  // Now we need to encode our IQ component in the carrier wave at the correct phase
  float2 s, c;
  sincos(2.0 * pi * phase, s, c);

  float2 luma = Y.xx;
  float2 chroma = s * I - c * Q;

  // If compositeBlend is 1, this is a composite output and we combine the whole signal into the one channel. Otherwise, use two.
  if (g_compositeBlend > 0)
  {
    g_outputTexture[dispatchThreadID] = (luma + chroma).xyxy;
  }
  else
  {
    g_outputTexture[dispatchThreadID] = float4(luma, chroma).xzyw;
  }

}