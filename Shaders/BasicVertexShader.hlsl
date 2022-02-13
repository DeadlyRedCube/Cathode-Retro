struct VSOutput
{
  float2 tex : TEX;
  float4 pos : SV_POSITION;
};


void main(float2 pos : POSITION, out VSOutput output)
{
  output.pos = float4((pos * 2 - 1) * float2(1, -1), 0, 1);
  output.tex = pos;
}
