// Do a barrel distortion (and horizontal noise wobble) to a given texture coordinate
float2 DistortCRTCoordinates(float2 tex, float2 distortion)
{
  static const float k_viewAspect = 16.0 / 9.0;
  static const float k_piOver4 = 0.78539816339;
  static const float k_hFOV = 1.0471975512; // 60 degrees

  float z = cos(tex.x * distortion.x * k_piOver4) * cos(tex.y * distortion.y * k_piOver4);

  float maxY = 1.0 / cos(distortion.y * k_piOver4);
  float maxX = 1.0 / cos(distortion.x * k_piOver4);

  tex /= z;
  tex /= float2(maxX, maxY);
  return tex;
}