#pragma once

#include "CathodeRetro/Settings.h"

namespace CathodeRetro
{
  // A preset structure that wraps a settings type with a name.
  template <typename T>
  struct Preset
  {
    const char *name;
    T settings;
  };


  static constexpr Preset<SourceSettings> k_sourcePresets[] =
  {
    {
      // Timings for NES- and SNES-like video generation
      "NES/SNES",
      {
        .inputPixelAspectRatio = 8.0f/7.0f, // NES pixels were 8:7
        .denominator = 3,                   // NES timings are all in multiples of 1/3rd
        .colorCyclesPerInputPixel = 2,      // NES has 2/3rds of a color phase for every pixel
                                            //  (i.e. it has a 33% greater horizontal resolution than the color signal can represent)
        .initialFramePhase = 0,             // The starting phase of the NES doesn't really matter, so just pick 0
        .phaseIncrementPerLine = 1,         // Every line the phase of the color cycle offsets by 1/3rd of the color
        .phaseIncrementPerEvenFrame = 2,    // On even frames, the color cycle offsets by 2/3rds of the color cycle
        .phaseIncrementPerOddFrame = 1,     // On odd frames, the color cycle offsets by the remaining 1/3rd of the color cycle
      }
    },
    {
      // Timings for NES- and SNES-like video generation (but with square pixels)
      "NES/SNES (Square Pixels)",
      {
        .inputPixelAspectRatio = 1.0f,    // Square pixels

        // The rest are the same as the NES/SNES preset.
        .denominator = 3,
        .colorCyclesPerInputPixel = 2,
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 1,
        .phaseIncrementPerEvenFrame = 2,
        .phaseIncrementPerOddFrame = 1,
      }
    },
    {
      // Timings for NES- and SNES-like video generation
      "SNES 512 mode",
      {
        .inputPixelAspectRatio = 4.0f/7.0f, // SNES 512 mode pixels were 4:7 per field (half-width relative to its standard 256 mode)
        .denominator = 3,                   // SNES timings are all in multiples of 1/3rd
        .colorCyclesPerInputPixel = 1,      // SNES 512 has 1/3rds of a color phase for every pixel

        // The following are identical to the other NES/SNES presets.
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 1,
        .phaseIncrementPerEvenFrame = 2,
        .phaseIncrementPerOddFrame = 1,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x240",
      {
        .inputPixelAspectRatio = 1.0f,      // Square pixels
        .denominator = 2,                   // CGA deals in multiples of 1/2
        .colorCyclesPerInputPixel = 1,      // Every pixel is half of a color subcarrier wave

        // CGA has no phase changes at all per line or frame
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 0,
        .phaseIncrementPerEvenFrame = 0,
        .phaseIncrementPerOddFrame = 0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x200",
      {
        .inputPixelAspectRatio = 5.0f/6.0f, // mode 13 and similar had slightly tall pixels

        // Otherwise identical to "PC Composite 320x240"
        .denominator = 2,
        .colorCyclesPerInputPixel = 1,
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 0,
        .phaseIncrementPerEvenFrame = 0,
        .phaseIncrementPerOddFrame = 0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x480",
      {
        .inputPixelAspectRatio = 1.0f,      // Square pixels
        .denominator = 4,                   // CGA at 640 pixels wide deals in multiples of 1/4
        .colorCyclesPerInputPixel = 1,      // Every pixel is a quarter a color subcarrier wave
                                            //  (half of the 320 span since we have twice the number of pixels)

        // CGA has no phase changes at all per line or frame
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 0,
        .phaseIncrementPerEvenFrame = 0,
        .phaseIncrementPerOddFrame = 0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x400",
      {
        .inputPixelAspectRatio = 5.0f/6.0f, // 640x400 had slightly tall pixels, same as 320x200

        // Otherwise identical to "PC Composite 640x480"
        .denominator = 4,
        .colorCyclesPerInputPixel = 1,
        .initialFramePhase = 0,
        .phaseIncrementPerLine = 0,
        .phaseIncrementPerEvenFrame = 0,
        .phaseIncrementPerOddFrame = 0,
      }
    },
  };


  static constexpr Preset<ArtifactSettings> k_artifactPresets[] =
  {
    {
      "Pristine",
      {
        .ghostVisibility = 0.0f,
        .ghostSpreadScale = 0.0f,
        .ghostDistance = 0.0f,
        .noiseStrength = 0.0f,
        .instabilityScale = 0.0f,
        .temporalArtifactReduction =  1.0f,
      },
    },
    {
      "Basic Analogue",
      {
        .ghostVisibility = 0.0f,
        .ghostSpreadScale = 0.0f,
        .ghostDistance = 0.0f,
        .noiseStrength = 0.025f,
        .instabilityScale = 0.375f,
        .temporalArtifactReduction =  1.0f,
      },
    },
    {
      "Basic Analogue (w/temporal artifacts)",
      {
        .ghostVisibility = 0.0f,
        .ghostSpreadScale = 0.0f,
        .ghostDistance = 0.0f,
        .noiseStrength = 0.025f,
        .instabilityScale = 0.375f,
        .temporalArtifactReduction =  0.0f,
      },
    },
    {
      "Bad Reception",
      {
        .ghostVisibility = 0.35f,
        .ghostSpreadScale = 0.90f,
        .ghostDistance = 3.00f,
        .noiseStrength = 0.20f,
        .instabilityScale = 1.25f,
        .temporalArtifactReduction =  0.0f,
      },
    },
  };


  static constexpr Preset<ScreenSettings> k_screenPresets[] =
  {
    {
      "Nothing At All",
      {
        .horizontalDistortion = 0.0f,
        .verticalDistortion = 0.0f,
        .screenEdgeRoundingX = 0.0f,
        .screenEdgeRoundingY = 0.0f,
        .cornerRounding = 0.0f,
        .shadowMaskScale = 1.0f,
        .shadowMaskStrength = 0.0f,
        .phosphorPersistence = 0.0f,
        .scanlineStrength = 0.0f,
        .diffusionStrength = 0.0f,
      }
    },
    {
      "Scanlines Only",
      {
        .horizontalDistortion = 0.0f,
        .verticalDistortion = 0.0f,
        .screenEdgeRoundingX = 0.0f,
        .screenEdgeRoundingY = 0.0f,
        .cornerRounding = 0.0f,
        .shadowMaskScale = 1.0f,
        .shadowMaskStrength = 0.0f,
        .phosphorPersistence = 0.25f,
        .scanlineStrength = 0.47f,
        .diffusionStrength = 0.0f,
      }
    },
    {
      "Flat CRT",
      {
        .horizontalDistortion = 0.0f,
        .verticalDistortion = 0.0f,
        .screenEdgeRoundingX = 0.0f,
        .screenEdgeRoundingY = 0.0f,
        .cornerRounding = 0.0f,
        .shadowMaskScale = 1.0f,
        .shadowMaskStrength = 0.65f,
        .phosphorPersistence = 0.25f,
        .scanlineStrength = 0.47f,
        .diffusionStrength = 0.5f,
      }
    },
    {
      "Standard CRT",
      {
        .horizontalDistortion = 0.35f,
        .verticalDistortion = 0.25f,
        .screenEdgeRoundingX = 0.0f,
        .screenEdgeRoundingY = 0.0f,
        .cornerRounding = 0.03f,
        .shadowMaskScale = 1.0f,
        .shadowMaskStrength = 0.65f,
        .phosphorPersistence = 0.25f,
        .scanlineStrength = 0.47f,
        .diffusionStrength = 0.5f,
      }
    },
    {
      "Standard CRT (No Scanlines)",
      {
        .horizontalDistortion = 0.35f,
        .verticalDistortion = 0.25f,
        .screenEdgeRoundingX = 0.0f,
        .screenEdgeRoundingY = 0.0f,
        .cornerRounding = 0.03f,
        .shadowMaskScale = 1.0f,
        .shadowMaskStrength = 0.65f,
        .phosphorPersistence = 0.25f,
        .scanlineStrength = 0.00f,
        .diffusionStrength = 0.5f,
      }
    },
    {
      "Old CRT",
      {
        .horizontalDistortion = 0.30f,
        .verticalDistortion = 0.40f,
        .screenEdgeRoundingX = 0.3f,
        .screenEdgeRoundingY = 0.15f,
        .cornerRounding = 0.17f,
        .shadowMaskScale = 1.2f,
        .shadowMaskStrength = 0.65f,
        .phosphorPersistence = 0.25f,
        .scanlineStrength = 0.63f,
        .diffusionStrength = 0.5f,
      }
    },
  };



}