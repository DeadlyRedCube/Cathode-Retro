struct PSInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEX;
};


void main(float2 pos : POSITION, out PSInput output)
{
  output.pos = float4((pos * 2 - 1) * float2(1, -1), 0, 1);
  output.tex = pos * 2 - 1;
}
