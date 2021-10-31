#pragma once

#include "NTSCContext.h"

namespace NTSC
{
  class GeneratorBase
  {
  public:
    virtual ~GeneratorBase() = default;

    // Take the input pixels (That this generator knows how to read) and convert them into a luma and chroma signal (like for S-Video)
    virtual void ProcessScanlineToLumaAndChroma(
      const void *inputPixels,
      const Context &context,
      f32 noiseScale,
      AlignedVector<f32> *lumaSignalOut,
      AlignedVector<f32> *chromaSignalOut) = 0;

    // Take the input pixels and convert them into a composite signal (that needs YC Separation)
    void ProcessScanlineToComposite(
      const void *inputPixels,
      const Context &context,
      f32 noiseScale,
      AlignedVector<f32> *compositeSignalOut,
      AlignedVector<f32> *scratchBuffer)
    {
      ProcessScanlineToLumaAndChroma(inputPixels, context, noiseScale, compositeSignalOut, scratchBuffer);

      // To get a composite signal, we just ... composite them. A sum, in other words.
      for (u32 x = 0; x < context.OutputTexelCount(); x++)
      {
        (*compositeSignalOut)[x] += (*scratchBuffer)[x];
      }
    }
  };
}
