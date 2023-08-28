#pragma once

#include <cinttypes>

namespace NTSCify
{
  enum class SignalType
  {
    RGB,          // Perfect RGB end to end.
    SVideo,       // Keep luma and chroma separate - meaning you get chroma modulation artifacting but not channel mixing artifacts.
    Composite,    // Blend luma and chroma, necessitating a separation pass which will introduce channel mixing artifacts
  };


  // This structure contains the settings used for the generation of the actual clean SVideo/composite signal, and represent the properties of the source of the 
  //  signal (i.e. the "machine" that is generating the signal)
  struct SourceSettings
  {
    // The display aspect ratio of an input pixel when displayed.
    //  Use 1/1 for square input pixels, but for the NES/SNES it's 8/7
    float inputPixelAspectRatio = 1.0f;     

    // This is the common denominator of all phase generation values (kept as a fraction to maintain numerical precision, because in
    //  practice they're all rational values). So if the denominator is 3, a value of 1 would be 1/3rd of a color cycle, 2 would be 
    //  2/3rds, etc. 
    //
    // Basically, all subsequent values are fractions with this as the denominator.
    uint32_t denominator = 1;

    // This is a measure of how many cycles of the color carrier wave there are per pixel, and the answer is usually <= 1.
    uint32_t colorCyclesPerInputPixel = 1;

    // Phase is measured in multiples of the color cycle.

    // This is what fraction of the color cycle the first line of the first frame starts at
    uint32_t initialFramePhase = 0;

    // This is what fraction of the color cycle the phase increments every scanline
    uint32_t phaseIncrementPerLine = 0;

    // Some systems had different phase changes per frame, and so this breaks down "how different is the starting phase of a frame
    //  from one frame to the next" into even and odd frame deltas.
    uint32_t phaseIncrementPerEvenFrame = 0;
    uint32_t phaseIncrementPerOddFrame = 0;
  };


  struct SourceSettingsPreset
  {
    const char *name;
    SourceSettings settings;
  };

  static constexpr SourceSettingsPreset k_sourcePresets[] =
  {
    {
      // Timings for NES- and SNES-like video generation
      "NES/SNES",
      {
        8.0f/7.0f,  // NES pixels were 8:7
        3,          // NES timings are all in multiples of 1/3rd 
        2,          // NES has 2/3rds of a color phase for every pixel (i.e. it has a 33% greater horizontal resolution than the color signal can represent)
        0,          // The starting phase of the NES doesn't really matter, so just pick 0
        1,          // Every line the phase of the color cycle offsets by 1/3rd of the color
        2,          // On even frames, the color cycle offsets by 2/3rds of the color cycle
        1,          // On odd frames, the color cycle offsets by 1/3rd of the color cycle (making the frames alternate, since 2/3 + 1/3 == 1)
      }
    },
    {
      // Timings for NES- and SNES-like video generation (but with square pixels)
      "NES/SNES (Square Pixels)",
      {
        1.0f,       // NES pixels were 8:7
        3,          // NES timings are all in multiples of 1/3rd 
        2,          // NES has 2/3rds of a color phase for every pixel (i.e. it has a 33% greater horizontal resolution than the color signal can represent)
        0,          // The starting phase of the NES doesn't really matter, so just pick 0
        1,          // Every line the phase of the color cycle offsets by 1/3rd of the color
        2,          // On even frames, the color cycle offsets by 2/3rds of the color cycle
        1,          // On odd frames, the color cycle offsets by 1/3rd of the color cycle (making the frames alternate, since 2/3 + 1/3 == 1)
      }
    },
    {
      // Timings for NES- and SNES-like video generation
      "SNES 512 mode",
      {
        4.0f/7.0f,  // SNES 512 mode pixels were 4:7 (assuming non-interlaced)
        3,          // SNES timings are all in multiples of 1/3rd 
        1,          // SNES 512 has 1/3rds of a color phase for every pixel
        0,          // The starting phase of the SNES doesn't really matter, so just pick 0
        1,          // Every line the phase of the color cycle offsets by 1/3rd of the color
        2,          // On even frames, the color cycle offsets by 2/3rds of the color cycle
        1,          // On odd frames, the color cycle offsets by 1/3rd of the color cycle (making the frames alternate, since 2/3 + 1/3 == 1)
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x240",
      {
        1.0f,       // Square pixels
        2,          // CGA deals in multiples of 1/2
        1,          // Every pixel is half of a color subcarrier wave
        0,          // Start at phase 0
        0,          // The CGA doesn't change phase at all per line (or per frames)
        0,          //  ...
        0,          //  ...
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x200",
      {
        5.0f/6.0f,  // mode 13 and similar had slightly tall pixels
        2,          // CGA deals in multiples of 1/2
        1,          // Every pixel is half of a color subcarrier wave
        0,          // Start at phase 0
        0,          // The CGA doesn't change phase at all per line (or per frames)
        0,          //  ...
        0,          //  ...
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x480",
      {
        1.0f,       // Square pixels
        4,          // CGA at 640 pixels wide deals in multiples of 1/4
        1,          // Every pixel is a quarter a color subcarrier wave (half of the 320 span since we have twice the number of pixels)
        0,          // Start at phase 0
        0,          // The CGA doesn't change phase at all per line (or per frames)
        0,          //  ...
        0,          //  ...
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x400",
      {
        5.0f/6.0f,  // 640x480 had slightly tall pixels
        4,          // CGA at 640 pixels wide deals in multiples of 1/4
        1,          // Every pixel is a quarter a color subcarrier wave (half of the 320 span since we have twice the number of pixels)
        0,          // Start at phase 0
        0,          // The CGA doesn't change phase at all per line (or per frames)
        0,          //  ...
        0,          //  ...
      }
    },
  };
}