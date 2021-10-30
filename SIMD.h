#pragma once

#include <intrin.h>

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