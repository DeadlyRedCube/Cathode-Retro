#pragma once

#include "FIRFilter.h"
#include "NTSCYCSeparatorBase.h"

namespace NTSC
{
  // This is an old-TV style Y/C separator. Result is blurrier than newer TVs accomplished due to suboptimal filtering.
  class LowbandpassYCSeparator : public YCSeparatorBase
  {
  public:
    LowbandpassYCSeparator(const GenerationInfo &genInfo);

    void Separate(
      const Context &context,
      const AlignedVector<f32> &compositeIn,
      AlignedVector<f32> *lumaSignalOut,
      AlignedVector<f32> *chromaSignalOut) override final;
  
  private:
    FIRFilter lumaFilter;
    FIRFilter chromaFilter;
  };
}