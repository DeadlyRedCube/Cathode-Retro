#pragma once

#include "AlignedVector.h"
#include "BaseTypes.h"
#include "MathHelpers.h"
#include "SIMD.h"

namespace NTSC
{


struct GenerationInfo
{
  u32 inputScanlinePixelCount;                    // How many input pixels there are per scanline
  
  u32 outputOversampleAmount;                     // How many texels of output there are per pixel of input
  f32 colorCyclesPerInputPixel;                   // How many NTSC color subcarrier wave cycles there are per input pixel (usually < 1)
  u32 phaseStateCount;                            // How many different phase states there are for a given scanline
  u32 initialPhaseStateIndex;                     // Which phase state the first frame starts on
  u32 phaseStateIndexIncrementPerLine;            // How much to add to the phase state index every scanline
  u32 phaseStateIndexIncrementPerFrame;           // How much to add to the frame's starting phase state index per frame
};


constexpr GenerationInfo NESandSNES240pGenerationInfo = 
{ 
  256,              // NES has 256 horizontal pixels

  4,                // The NES needs at least this many output texels per pixel to get all the phases it can generate
  2.0f/3.0f,        // Every NES pixel covers 2/3rds of a color wave
  3,                // NES has three distinct phase states it can land in thanks to its 2/3-wave-per-pixel timing
  0,                // This doesn't REALLY matter, any value works here, so might as well start at 0.
  1,                // Every line we move by 1/3rd of a color wave, which is 1 phase state
  2,                // Every successive frame is 2 phase states off from the one before.
};


constexpr GenerationInfo PC320GenerationInfo = 
{ 
  320,              // PC 320 width mode has (of course) 320 pixels of width
  2,                // Oversample the output 2x
  1.0f/2.0f,        // EVery PC pixel is exactly half of a color subcarrier wave 
  2,                // PC really just has one phase state (starting halfway through the wave each frame) but I used too because of the "halfway" thing
  1,                // Always start halfway through the phase wave.
  0,                // Every line has the same starting phase as every other line
  0                 // Every frame, same
};


constexpr GenerationInfo PC640GenerationInfo = 
{ 
  640,              // PC 640 has twice the horizontal resolution as its 320 mode.
  1,                // Don't even need to oversample the output for this one
  1.0f/4.0f,        // Because it has twice the resolution, each pixel covers half of what a PC 320 pixel covers (1/4 wave)
  2,                // These values are identical to PC 320
  1,                // ...
  0,                // ...
  0,                // ...
};


class Context
{
public:
  explicit Context(const GenerationInfo &generationInfo)
  {
    // We have one output texel per "outputOversampleAmount" input pixels.
    outputTexelCount = generationInfo.inputScanlinePixelCount * generationInfo.outputOversampleAmount;
    genInfo = generationInfo;

    u32 tableSize = Math::AlignInt(outputTexelCount, k_maxFloatAlignment);

    // Generate the sin/cos tables
    for (u32 i = 0; i < generationInfo.phaseStateCount; i++)
    {
      // Each phase state starts with the next phase
      f32 phase = f32(i) / f32(generationInfo.phaseStateCount);

      // We increment phase a little bit per each output texel
      f32 phaseIncrement = generationInfo.colorCyclesPerInputPixel / f32(generationInfo.outputOversampleAmount);

      AlignedVector<f32> sinTable;
      AlignedVector<f32> cosTable;

      sinTable.resize(tableSize);
      cosTable.resize(tableSize);

      for (u32 x = 0; x < outputTexelCount; x++)
      {
        sinTable[x] = Math::SinPi(2.0f * phase);
        cosTable[x] = Math::CosPi(2.0f * phase);

        phase += phaseIncrement;
      }

      sinTables.push_back(std::move(sinTable));
      cosTables.push_back(std::move(cosTable));
    }
  }

  void StartFrame(s32 phaseIndex = -1)
  {
    if (phaseIndex >= 0)
    {
      // If we passed in an explicit phase index, use it
      frameStartPhaseIndex = u32(phaseIndex);
    }
    else
    {
      // Otherwise increment it according to our generation info
      frameStartPhaseIndex = (frameStartPhaseIndex + genInfo.phaseStateIndexIncrementPerFrame) % genInfo.phaseStateCount;
    }

    // Start the first line at the desired phase index
    currentLinePhaseIndex = frameStartPhaseIndex;
    scanlineIndex = 0;
  }

  void EndScanline()
  { 
    // Increment our scanline phase and counter
    currentLinePhaseIndex = (currentLinePhaseIndex + genInfo.phaseStateIndexIncrementPerLine) % genInfo.phaseStateCount; 
    scanlineIndex++; 
  }

  const AlignedVector<f32> &SinTable() const
    { return sinTables[currentLinePhaseIndex]; }

  const AlignedVector<f32> &CosTable() const
    { return cosTables[currentLinePhaseIndex]; }

  u32 OutputTexelCount() const
    { return outputTexelCount; }

  u32 ScanlineIndex() const
    { return scanlineIndex; }

  const GenerationInfo &GenInfo() const
    { return genInfo; }

protected:
  GenerationInfo genInfo;
  u32 frameStartPhaseIndex = 0;
  u32 currentLinePhaseIndex = 0;
  u32 outputTexelCount = 0;
  u32 scanlineIndex = 0;
  std::vector<AlignedVector<f32>> sinTables;
  std::vector<AlignedVector<f32>> cosTables;
};


}