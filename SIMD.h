#pragma once

#include <intrin.h>

namespace NTSC
{
  constexpr size_t k_maxFloatAlignment = 8;
  constexpr size_t k_maxAlignment = k_maxFloatAlignment * sizeof(float);

  enum class SIMDInstructionSet
  {
    None,
    AVX,
    AVX2,
  };

  inline SIMDInstructionSet GetInstructionSet()
  {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);

    auto idCount = cpuInfo[0];

    // Read some seemingly-arbitrary bits of __cpuid output to determine SIMD support
    SIMDInstructionSet set = SIMDInstructionSet::None;
    if (idCount >= 1)
    {
      __cpuid(cpuInfo, 1);
      if ((cpuInfo[2] & (1 << 28)) != 0)
      {
        set = SIMDInstructionSet::AVX;
      }
    }

    if (idCount >= 7)
    {
      __cpuid(cpuInfo, 7);
      if ((cpuInfo[1] & (1 << 5)) != 0)
      {
        set = SIMDInstructionSet::AVX2;
      }
    }

    return set;
  }

  inline SIMDInstructionSet k_maxInstructionSet = GetInstructionSet();
}