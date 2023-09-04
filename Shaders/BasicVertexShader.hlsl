///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is, as the name might have given you, a very basic vertex shader. It takes an input position in [0..1] and outputs that as the
//  texture coordinate, and scales/biases it to [-1..1] for the position. This is intended for rendering a full-render-texture quad.


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
