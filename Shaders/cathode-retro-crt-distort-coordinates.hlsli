// Do a barrel distortion to a given texture coordinate to emulate a curved CRT screen.

#include "cathode-retro-util-language-helpers.hlsli"

float2 DistortCRTCoordinates(
  // The original texture coordinate, intended to come straight from the full-render-target quad, in [-1..1] range (not standard [0..1])
  float2 texCoord,

  // a [horizontal, vertical] distortion pair which describes the effective curvature of the virtual screen.
  float2 distortion)
{
  const float k_piOver4 = 0.78539816339;

  // $TODO: I would love to actually graph these coordinates and see how correct they appear - it's possible this is not quite the right
  //  type of curvature used for CRT glass but I haven't found any easy references as to how a CRT screen's curve needed to work.

  // Calculate a z coordinate for a virtual screen.
  // $NOTE: If you wanted to generate a room reflection or something off of this, you could use this z coordinate (and the final x, y
  //  coordinates before the divide-by-z to do so)
  float z = cos(texCoord.x * distortion.x * k_piOver4) * cos(texCoord.y * distortion.y * k_piOver4);

  float maxY = 1.0 / cos(distortion.y * k_piOver4);
  float maxX = 1.0 / cos(distortion.x * k_piOver4);
  return texCoord / float2(maxX, maxY) / z;
}