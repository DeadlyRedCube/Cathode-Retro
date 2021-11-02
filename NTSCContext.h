#pragma once

#include "AlignedVector.h"
#include "BaseTypes.h"
#include "MathHelpers.h"
#include "SIMD.h"

namespace NTSC
{


struct Fractionish // It's not exactly a fraction because the numerator can be a float, so it's only fraction-ish
{
  constexpr Fractionish() = default;
  constexpr Fractionish(f32 num, u32 den = 1)
    : numerator(num)
    , denominator(den)
    { } 

  Fractionish operator + (const Fractionish &b) const
  {
    ASSERT(denominator == b.denominator);
    return {numerator + b.numerator, denominator};
  }

  Fractionish &operator += (const Fractionish &b)
    { ASSERT(denominator == b.denominator); numerator += b.numerator; return *this; }

  explicit operator f32() const
    { return numerator / f32(denominator); }

  void ModWithDenominator()
  {
    numerator = std::fmod(numerator, f32(denominator));
  }

  void SetDenominator(u32 newDen)
  {
    if ((newDen % denominator) == 0)
    {
      u32 mult = newDen / denominator;
      numerator *= f32(mult);
    }
    else
    {
      numerator *= f32(newDen) / f32(denominator);
    }

    denominator = newDen;
  }

  f32 numerator = 0.0f;   // Numerator is allowed to be negative and non-integral.
  u32 denominator = 1;    // Denominator needs to be integral. The whole point is to avoid float error on, say, fractions of 3.
};

struct GenerationInfo
{
  u32 inputScanlinePixelCount;                    // How many input pixels there are per scanline
  u32 outputOversampleAmount;                     // How many texels of output there are per pixel of input
  f32 pixelAspectRatio;                           // width/height of a pixel for the given display mode

  Fractionish colorCyclesPerInputPixel;           // How many cycles of the NTSC color subcarrier per input pixel (usually <= 1)
  Fractionish initialFramePhase;                  // The phase that the first frame starts at
  Fractionish perLinePhaseIncrement;              // How much the phase changes per scanline
  Fractionish perFramePhaseIncrement;             // How much the phase changes per frame
};


constexpr GenerationInfo NESandSNES240pGenerationInfo = 
{ 
  256,              // NES has 256 horizontal pixels
  4,                // The NES needs at least this many output texels per pixel to get all the phases it can generate
  8.0f / 7.0f,      // NES/SNES pixels are 8:7

  {2.0f, 3},        // Every NES pixel covers 2/3rds of a color wave
  0,                // This doesn't REALLY matter, any value works here, so might as well start at 0.
  {1.0f, 3},        // Every line we move by 1/3rd of a color wave
  {2.0f, 3},        // Every successive frame is 2/3rds of a phase off from the previous one
};


constexpr GenerationInfo Genesis256WideGenerationInfo = 
{ 
  256,              // This Genesis mode has 256 horizontal pixels
  2,                // Oversample!
  8.0f / 7.0f,      // This mode the pixels are 8:7, just like NES/SNES

  {2.0f, 3},        // Every pixel covers 2/3rds of a color wave
  0,                // This doesn't REALLY matter, any value works here, so might as well start at 0.
  0,                // Line phases for Genesis 256 are constant (I think)
  0.5f,             // Every successive frame is a half a phase off (I think, again)
};


constexpr GenerationInfo Genesis320WideGenerationInfo = 
{ 
  320,              // NES has 256 horizontal pixels
  2,                // Oversample!
  6.4f / 7.0f,      // The aspect ratio of the screen of this mode is the same as the 256-wide mode, so (8 * 256 / 320):7 is the pixel aspect

  {8.0f, 15},       // Every Genesis pixel covers just a liiitle more than half a wavelength (but less than a pixel at 256-wide does, since more pixels) 1.6/3 == 8/15
  0,                // This doesn't REALLY matter, any value works here, so might as well start at 0.
  0,                // Line phases for Genesis 256 are constant
  0.5f,             // Every successive frame is a half a phase off 
};


constexpr GenerationInfo PC320GenerationInfo = 
{ 
  320,              // PC 320 width mode has (of course) 320 pixels of width
  2,                // Oversample the output 2x
  1.0f,             // by default we treat 320-wide mode as having square pixels

  0.5f,             // Every PC pixel is exactly half of a color subcarrier wave
  0.5f,             // Always start halfway through the phase wave.
  0,                // Every line has the same starting phase as every other line
  0                 // Every frame, same
};


constexpr GenerationInfo PC640GenerationInfo = 
{ 
  640,              // PC 640 has twice the horizontal resolution as its 320 mode.
  1,                // Don't even need to oversample the output for this one
  1.0f,             // by default we treat 640-wide mode as having square pixels

  0.25f,            // Because it has twice the resolution, each pixel covers half of what a PC 320 pixel covers (1/4 wave)
  0.5f,             // These values are identical to PC320
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

    // Figure out the least common multiple of the denominators (the least common denominator) and update our fractions
    u32 leastCommonMultiple = Math::LeastCommonMultiple(genInfo.colorCyclesPerInputPixel.denominator, genInfo.initialFramePhase.denominator);
    leastCommonMultiple = Math::LeastCommonMultiple(leastCommonMultiple, genInfo.perLinePhaseIncrement.denominator);
    leastCommonMultiple = Math::LeastCommonMultiple(leastCommonMultiple, genInfo.perFramePhaseIncrement.denominator);

    genInfo.colorCyclesPerInputPixel.SetDenominator(leastCommonMultiple);
    genInfo.initialFramePhase.SetDenominator(leastCommonMultiple);
    genInfo.perLinePhaseIncrement.SetDenominator(leastCommonMultiple);
    genInfo.perFramePhaseIncrement.SetDenominator(leastCommonMultiple);

    frameStartPhase = genInfo.initialFramePhase;

    u32 tableSize = Math::AlignInt(outputTexelCount, k_maxFloatAlignment);

    // Generate the sin/cos tables
    {
      // Each phase state starts with the next phase
      f32 phase = 0.0f;

      // We increment phase a little bit per each output texel
      f32 phaseIncrement = f32(generationInfo.colorCyclesPerInputPixel) / f32(generationInfo.outputOversampleAmount);

      sinTable.resize(tableSize);
      cosTable.resize(tableSize);

      for (u32 x = 0; x < outputTexelCount; x++)
      {
        sinTable[x] = Math::SinPi(2.0f * phase);
        cosTable[x] = Math::CosPi(2.0f * phase);

        phase += phaseIncrement;
      }
    }
  }

  void StartFrame(Fractionish startPhase = -1)
  {
    if (startPhase.numerator >= 0)
    {
      u32 lcm = Math::LeastCommonMultiple(startPhase.denominator, frameStartPhase.denominator);
      ASSERT(lcm == frameStartPhase.denominator);
      startPhase.SetDenominator(lcm);

      // If we passed in an explicit phase index, use it
      frameStartPhase = startPhase;
    }
    else
    {
      // Otherwise increment it according to our generation info
      frameStartPhase += genInfo.perFramePhaseIncrement;
      frameStartPhase.ModWithDenominator();
    }

    // Start the first line at the desired phase index
    currentLinePhase = frameStartPhase;
    scanlineIndex = 0;
    linePhaseSin = Math::SinPi(2.0f * f32(currentLinePhase));
    linePhaseCos = Math::CosPi(2.0f * f32(currentLinePhase));
  }

  void EndScanline()
  { 
    // Increment our scanline phase and counter
    currentLinePhase += genInfo.perLinePhaseIncrement;
    currentLinePhase.ModWithDenominator();
    scanlineIndex++; 
    linePhaseSin = Math::SinPi(2.0f * f32(currentLinePhase));
    linePhaseCos = Math::CosPi(2.0f * f32(currentLinePhase));
  }

  u32 OutputTexelCount() const
    { return outputTexelCount; }

  u32 ScanlineIndex() const
    { return scanlineIndex; }

  const GenerationInfo &GenInfo() const
    { return genInfo; }

  f32 Sin(u32 x) const
    { return sinTable[x] * linePhaseCos + cosTable[x] * linePhaseSin; } // sin(a + b) where "a" is current pixel phase (via x and the lookup) and "b" is our line phase

  f32 Cos(u32 x) const
    { return cosTable[x] * linePhaseCos - sinTable[x] * linePhaseSin; } // cos(a + b) where "a" is current pixel phase (via x and the lookup) and "b" is our line phase

protected:
  GenerationInfo genInfo;
  Fractionish frameStartPhase = 0;
  Fractionish currentLinePhase = 0;
  u32 outputTexelCount = 0;
  u32 scanlineIndex = 0;
  AlignedVector<f32> sinTable;
  AlignedVector<f32> cosTable;

  f32 linePhaseSin = 0.0f;
  f32 linePhaseCos = 1.0f;
};


}