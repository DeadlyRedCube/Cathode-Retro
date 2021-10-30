#pragma once

#include <vector>

#include "SIMD.h"

namespace NTSC
{
  template <typename Type>
  class AlignAllocator
  {
  public:
    using value_type = Type;
  
    constexpr AlignAllocator() noexcept = default;
    template <typename T>
    constexpr AlignAllocator(const AlignAllocator<T> &) noexcept {};

    Type *allocate(size_t n)
      { return static_cast<Type *>(_mm_malloc(n * sizeof(Type), std::max(alignof(Type), k_maxAlignment))); }

    void deallocate(const Type *p, [[maybe_unused]] const size_t n) noexcept
      { _mm_free(const_cast<Type *>(p)); }

    inline bool operator == (const AlignAllocator &) 
      { return true; }

    inline bool operator != (const AlignAllocator &) 
      { return false; }
  };

  template <typename Type>
  using AlignedVector = std::vector<Type, AlignAllocator<Type>>;
}