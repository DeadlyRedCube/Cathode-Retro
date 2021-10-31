#pragma once

#include "NTSCContext.h"

namespace NTSC
{
  class YCSeparatorBase
  {
  public:
    virtual ~YCSeparatorBase() = default;

    // Take a composite NTSC signal and separate out the Y and C (luma and chroma) signals
    virtual void Separate(
      const Context &context,
      const AlignedVector<f32> &compositeIn,
      AlignedVector<f32> *lumaSignalOut,
      AlignedVector<f32> *chromaSignalOut) = 0;
  };
}