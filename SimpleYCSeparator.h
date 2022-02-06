#pragma once

#include "NTSCYCSeparatorBase.h"

namespace NTSC
{
  class SimpleYCSeparator : public YCSeparatorBase
  {
  public:
    SimpleYCSeparator(const GenerationInfo &genInfo)
    {
      // We want the rolling window to be this many color cycles
      float colorCycleCount = 1.0f;

      f32 colorCyclesPerOutputPixel = f32(genInfo.colorCyclesPerInputPixel) / f32(genInfo.outputOversampleAmount);
      uint32_t windowSize = uint32_t(std::round(colorCycleCount / colorCyclesPerOutputPixel));
      rollingWindow.resize(windowSize);
    }


    virtual ~SimpleYCSeparator() = default;

    
    // Take a composite NTSC signal and separate out the Y and C (luma and chroma) signals
    virtual void Separate(
      const Context &context,
      const AlignedVector<f32> &compositeIn,
      AlignedVector<f32> *lumaSignalOut,
      AlignedVector<f32> *chromaSignalOut) override final
    {
      memset(rollingWindow.data(), 0, rollingWindow.size() * sizeof(32));
      avgUnscaled = 0.0f;
      rollingWindowNextIndex = 0;

      for (size_t x = 0; x < context.OutputTexelCount(); x++)
      {
        avgUnscaled -= rollingWindow[rollingWindowNextIndex];
        rollingWindow[rollingWindowNextIndex] = compositeIn[std::min(compositeIn.size() - 1, size_t(x + rollingWindow.size() / 2))];
        avgUnscaled += rollingWindow[rollingWindowNextIndex];


        //avgUnscaled = 0;
        //for (size_t i = 0; i < rollingWindow.size(); i++)
        //{
        //  avgUnscaled += rollingWindow[i];
        //}

        f32 luma = avgUnscaled / rollingWindow.size();
        (*lumaSignalOut)[x] = luma;
        (*chromaSignalOut)[x] = compositeIn[x] - luma;
        rollingWindowNextIndex = (rollingWindowNextIndex + 1) % rollingWindow.size();
      }
    }

  private:
    std::vector<f32> rollingWindow;
    int rollingWindowNextIndex = 0;
    f32 avgUnscaled = 0.0f;
  };
}