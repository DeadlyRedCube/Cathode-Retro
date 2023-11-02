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
        8.0f/7.0f,  // NES pixels were 8:7
        3,          // NES timings are all in multiples of 1/3rd
        2,          // NES has 2/3rds of a color phase for every pixel
                    //  (i.e. it has a 33% greater horizontal resolution than the color signal can represent)
        0,          // The starting phase of the NES doesn't really matter, so just pick 0
        1,          // Every line the phase of the color cycle offsets by 1/3rd of the color
        2,          // On even frames, the color cycle offsets by 2/3rds of the color cycle
        1,          // On odd frames, the color cycle offsets by the remaining 1/3rd of the color cycle
      }
    },
    {
      // Timings for NES- and SNES-like video generation (but with square pixels)
      "NES/SNES (Square Pixels)",
      {
        1.0f,       // Square pixels

        // The rest are the same as the NES/SNES preset.
        3,
        2,
        0,
        1,
        2,
        1,
      }
    },
    {
      // Timings for NES- and SNES-like video generation
      "SNES 512 mode",
      {
        4.0f/7.0f,  // SNES 512 mode pixels were 4:7 per field (half-width relative to its standard 256 mode)
        3,          // SNES timings are all in multiples of 1/3rd
        1,          // SNES 512 has 1/3rds of a color phase for every pixel

        // The following are identical to the other NES/SNES presets.
        0,
        1,
        2,
        1,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x240",
      {
        1.0f,       // Square pixels
        2,          // CGA deals in multiples of 1/2
        1,          // Every pixel is half of a color subcarrier wave

        // CGA has no phase changes at all per line or frame
        0,
        0,
        0,
        0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 320px horizontal resolution
      "PC Composite 320x200",
      {
        5.0f/6.0f,  // mode 13 and similar had slightly tall pixels

        // Otherwise identical to "PC Composite 320x240"
        2,
        1,
        0,
        0,
        0,
        0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x480",
      {
        1.0f,       // Square pixels
        4,          // CGA at 640 pixels wide deals in multiples of 1/4
        1,          // Every pixel is a quarter a color subcarrier wave (half of the 320 span since we have twice the number of pixels)

        // CGA has no phase changes at all per line or frame
        0,
        0,
        0,
        0,
      }
    },
    {
      // Timings for CGA (And likely other PC board)-like video generation, 640px horizontal resolution
      "PC Composite 640x400",
      {
        5.0f/6.0f,  // 640x400 had slightly tall pixels, same as 320x200

        // Otherwise identical to "PC Composite 640x480"
        4,
        1,
        0,
        0,
        0,
        0,
      }
    },
  };


  static constexpr Preset<ArtifactSettings> k_artifactPresets[] =
  {
    {
      "Pristine",
      {
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
      },
    },
    {
      "Basic Analogue",
      {
        0.0f,
        0.0f,
        0.0f,
        0.025f,
        0.375f,
        1.0f,
      },
    },
    {
      "Basic Analogue (w/temporal artifacts)",
      {
        0.0f,
        0.0f,
        0.0f,
        0.025f,
        0.375f,
        0.0f,
      },
    },
    {
      "Bad Reception",
      {
        0.35f,
        0.90f,
        3.00f,
        0.20f,
        1.25f,
        0.0f,
      },
    },
  };


  static constexpr Preset<ScreenSettings> k_screenPresets[] =
  {
    {
      "Nothing At All",
      {
        { 0.0f, 0.0f },           // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.0f,                     // cornerRounding
        MaskType::SlotMask,       // maskType
        1.0f,                     // maskScale
        0.0f,                     // maskStrength
        0.0f,                     // phosphorPersistence
        0.0f,                     // scanlineStrength
        0.0f,                     // diffusionStrength
      }
    },
    {
      "Scanlines Only",
      {
        { 0.0f, 0.0f },           // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.0f,                     // cornerRounding
        MaskType::SlotMask,       // maskType
        1.0f,                     // maskScale
        0.0f,                     // maskStrength
        0.25f,                    // phosphorPersistence
        0.47f,                    // scanlineStrength
        0.0f,                     // diffusionStrength
      }
    },
    {
      "Flat CRT",
      {
        { 0.0f, 0.0f },           // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.0f,                     // cornerRounding
        MaskType::SlotMask,       // maskType
        1.2f,                     // maskScale
        0.42f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.4f,                     // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Flat CRT (No Scanlines)",
      {
        { 0.0f, 0.0f },           // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.0f,                     // cornerRounding
        MaskType::SlotMask,       // maskType
        1.2f,                     // maskScale
        0.85f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.0f,                     // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Standard CRT",
      {
        { 0.25f, 0.15f },         // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.03f,                    // cornerRounding
        MaskType::SlotMask,       // maskType
        1.2f,                     // maskScale
        0.42f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.4f,                     // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Standard CRT (No Scanlines)",
      {
        { 0.25f, 0.15f },         // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.03f,                    // cornerRounding
        MaskType::SlotMask,       // maskType
        1.2f,                     // maskScale
        0.85f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.00f,                    // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Trin CRT",
      {
        { 0.15f, 0.0f },          // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.03f,                    // cornerRounding
        MaskType::ApertureGrille, // maskType
        1.2f,                     // maskScale
        0.42f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.4f,                     // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Trin CRT (No Scanlines)",
      {
        { 0.15f, 0.0f },          // distortion
        { 0.0f, 0.0f },           // screenEdgeRounding
        0.03f,                    // cornerRounding
        MaskType::ApertureGrille, // maskType
        1.2f,                     // maskScale
        0.85f,                    // maskStrength
        0.25f,                    // phosphorPersistence
        0.00f,                    // scanlineStrength
        0.5f,                     // diffusionStrength
      }
    },
    {
      "Old CRT",
      {
        { 0.35f, 0.30f },         // distortion
        { 0.15f, 0.10f },         // screenEdgeRounding
        0.12f,                    // cornerRounding
        MaskType::ShadowMask,     // maskType
        1.15f,                    // maskScale
        0.3f,                     // maskStrength
        0.25f,                    // phosphorPersistence
        0.4f,                     // scanlineStrength
        0.7f,                     // diffusionStrength
      }
    },
  };
}