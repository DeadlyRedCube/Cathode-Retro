// Perform a centered box filter - which means we might need to sample a half-texel off of either end of the filter, if the filter width is
//  even (and it probably is).
float2 BoxFilter(
  Texture2D<float2> sourceTexture,
  sampler samp,
  float2 invTextureSize,
  uint filterWidth,
  float2 texCoord,
  out float2 centerSample)
{
  // Get the center sample (which we'll write out to the caller)
  centerSample = sourceTexture.Sample(samp, texCoord);

  // Average starts with the center sample
  float2 avg = centerSample;

  // Excluding the center texel, iterate over every two samples to either side of the center sampling in the middle to get a nice bilinear
  //  average of the two (getting two texel samples for free). Multiply by 2 so that we get 1.0 of each of them.
  int iterEnd = int((filterWidth - 1) / 2);
  for (int i = 2; i < iterEnd; i += 2)
  {
    avg += 2 * sourceTexture.Sample(samp, texCoord + float2(i - 0.5, 0) * invTextureSize);
    avg += 2 * sourceTexture.Sample(samp, texCoord + float2(0.5 - i, 0) * invTextureSize);
  }

  // Now we either have no texels, a half texel, a full texel, or 1.5 texels remaining (or in other words, our width-minus-center modulo 4
  //  is one of 0 through 3.  If it's 0, we don't have to do anything, we've averaged all of the texels.
  uint remainder = (filterWidth - 1) % 4;
  if (remainder == 3)
  {
    // If it's 3, we have 1.5 texels left per end. Sample 1/3rd of the way into the inner-most texel (Because of how lerp works, that means
    //  we get 2/3rd of the inner texel and 1/3rd of the outermost), then multiply the result by 1.5 to give us 1.0 of the innermost and
    //  0.5 of the outermost texel (we want one half of the outer texel and full weight of the inner one).
    avg += 1.5 * sourceTexture.Sample(samp, texCoord + float2(iterEnd + 1.0 / 3.0, 0) * invTextureSize);
    avg += 1.5 * sourceTexture.Sample(samp, texCoord + float2(-iterEnd - 1.0 / 3.0, 0) * invTextureSize);
  }
  else if (remainder > 0)
  {
    // If the remainder is either 1 or 2, we either have a half texel or a full texel remaining on either end. Sample the center of that
    //  texel and then scale it down by 0.5 if we only need half of its weight.
    float scale = (remainder == 2) ? 1 : 0.f;
    avg += scale * sourceTexture.Sample(samp, texCoord + float2(iterEnd + 1.0, 0) * invTextureSize);
    avg += scale * sourceTexture.Sample(samp, texCoord + float2(-iterEnd - 1.0, 0) * invTextureSize);
  }

  return avg / float(filterWidth);
}