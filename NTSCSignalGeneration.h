#pragma once

#include <cinttypes>
#include <cmath>

#include "SimpleArray.h"


static constexpr int32_t k_signalSamplesPerColorCycle = 4; // No matter what timings are generating the signal, always aim for this many samples per color cycle


// A structure that contains auto-emulation properties of an ideal system (i.e. NES-like or Genesis-like or CGA-like, or whatever)
struct NTSCPhaseGenerationInfo
{
  // This is the common denominator of all phase generation values (kept as a fraction to maintain numerical precision, because in
  //  practice they're all rational values). So if the denominator is 3, a value of 1 would be 1/3rd of a color cycle, 2 would be 
  //  2/3rds, etc. 
  //
  // Basically, all subsequent values are fractions with this as the denominator.
  int32_t denominator;

  // This is a measure of how many cycles of the color carrier wave there are per pixel, and the answer is usually <= 1.
  int32_t colorCyclesPerInputPixel;

  // Phase is measured in multiples of the color cycle.

  // This is what fraction of the color cycle the first line of the first frame starts at
  int32_t initialFramePhase;

  // This is what fraction of the color cycle the phase increments every scanline
  int32_t phaseIncrementPerLine;

  // Some systems had different phase changes per frame, and so this breaks down "how different is the starting phase of a frame
  //  from one frame to the next" into even and odd frame deltas.
  int32_t phaseIncrementPerEvenFrame;
  int32_t phaseIncrementPerOddFrame;
};


static constexpr NTSCPhaseGenerationInfo k_pixelPerfectGenerationInfo =
{
  1,
  1,
  0,
  0,
  0,
  0,
};


// Timings for NES and SNES-like video generation
static constexpr NTSCPhaseGenerationInfo k_NESLikePhaseGenerationInfo =
{
  3,        // NES timings are all in multiples of 1/3rd 
  2,        // NES has 2/3rds of a color phase for every pixel (i.e. it has a 33% greater horizontal resolution than the color signal can represent)
  0,        // The starting phase of the NES doesn't really matter, so just pick 0
  1,        // Every line the phase of the color cycle offsets by 1/3rd of the color
  2,        // On even frames, the color cycle offsets by 2/3rds of the color cycle
  1,        // On odd frames, the color cycle offsets by 1/3rd of the color cycle (making the frames alternate, since 2/3 + 1/3 == 1)
};


// Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
static constexpr NTSCPhaseGenerationInfo k_CGA320LikePhaseGenerationInfo =
{
  2,        // CGA deals in multiples of 1/2
  1,        // Every pixel is half of a color subcarrier wave
  0,        // Start halfway into the phase ($TODO this could be wrong, check this when actually building a CGA-like signal generator)
  0,        // The CGA doesn't change phase at all per line (or per frames)
  0,        //  ...
  0,        //  ...
};


// Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
static constexpr NTSCPhaseGenerationInfo k_CGA640LikePhaseGenerationInfo =
{
  4,        // CGA deals in multiples of 1/2
  1,        // Every pixel is a quarter a color subcarrier wave
  2,        // Start halfway into the phase ($TODO this could be wrong, check this when actually building a CGA-like signal generator)
  0,        // The CGA doesn't change phase at all per line (or per frames)
  0,        //  ...
  0,        //  ...
};


static constexpr NTSCPhaseGenerationInfo k_genesis320WideGenerationInfo = 
{ 
  15,
  8,               // Every Genesis pixel covers just a liiitle more than half a wavelength (but less than a pixel at 256-wide does, since more pixels) 1.6/3 == 8/15
  0,                // This doesn't REALLY matter, any value works here, so might as well start at 0.
  0,                // Line phases for Genesis 256 are constant
  0,                // Every successive frame is a half a phase off 
};


// $TODO Figure out the timings for the Genesis to get them in here, too


struct NTSCFrameInfo
{
  bool isEvenFrame = true;
  int32_t currentFramePhaseOffset = 0;
};