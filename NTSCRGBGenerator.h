#pragma once

#include "Debug.h"
#include "NTSCGeneratorBase.h"

namespace NTSC
{
  // Takes a 32-bit signal (0x--BBGGRR) and turns it into an NTSC luma and chroma signal
  class RGBGenerator : public GeneratorBase
  {
  protected:
    void ProcessScanlineToLumaAndChromaInternal(
      const void *inputPixels,
      const Context &context,
      AlignedVector<f32> *lumaSignalOut,
      AlignedVector<f32> *chromaSignalOut) override final
    {
      auto &&gen = context.GenInfo();

      const u32 *rgbPixels = static_cast<const u32 *>(inputPixels);
      s32 phaseIndex = 0;

      f32 *lumaSignalDst = lumaSignalOut->data();
      f32 *chromaSignalDst = chromaSignalOut->data();
      for (u32 x = 0; x < gen.inputScanlinePixelCount; x++) // Iterate over the input pixels
      {
        u32 abgr = rgbPixels[x];

        // Decode u32 to rgb
        u8 r8 = u8(abgr  & 0x000000FF);
        u8 g8 = u8((abgr & 0x0000FF00) >> 8);
        u8 b8 = u8((abgr & 0x00FF0000) >> 16);

        f32 r = f32(r8) / 255.0f;
        f32 g = f32(g8) / 255.0f;
        f32 b = f32(b8)/ 255.0f;
    
        // Convert RGB to YIQ using a standard rgb -> YIQ matrix
        f32 y = 0.3000f * r + 0.5900f * g + 0.1100f * b;
        f32 i = 0.5990f * r - 0.2773f * g - 0.3217f * b;
        f32 q = 0.2130f * r - 0.5251f * g + 0.3121f * b;

        // Generate the signal for this pixel into the output
        for (u32 s = 0; s < gen.outputOversampleAmount; s++)
        {
          // Luma signal is easy: it's just the y component
          *lumaSignalDst = y;

          // Chroma signal requires modulating i and q with our carrier waves (it's a QAM encoding) and summing them
          *chromaSignalDst = -context.Sin(phaseIndex) * i + context.Cos(phaseIndex) * q;

          lumaSignalDst++;
          chromaSignalDst++;
          phaseIndex++;
        }
      }
    }
  };
}