#pragma once

#include <algorithm>

#include "ButterworthIIR.h"
#include "NTSCContext.h"
#include "NTSCData.h"

namespace NTSC
{
  class SignalDecoder
  {
  public:
    enum class FilterType
    {
      IIR,
      FIR,
      Simple,
    };

    SignalDecoder(const GenerationInfo &genInfo, FilterType filterTypeIn = FilterType::IIR)
    {
      f32 colorBurstCycleCountPerOutputSample = f32(genInfo.colorCyclesPerInputPixel) / f32(genInfo.outputOversampleAmount);
      filterType = filterTypeIn;

      // Our demodulation filter is a low pass IIR. Make sure to have it get the latency
      switch (filterType)
      {
      case FilterType::IIR:
        demodulateFilterIIR = ButterworthIIR::Filter::CreateLowPass(3, 1.2f * colorBurstCycleCountPerOutputSample);
        demodulateFilterIIR.MeasureLatency();
        break;

      case FilterType::FIR:
        demodulateFilterFIR = FIRFilter::CreateLowPass(1.5f * colorBurstCycleCountPerOutputSample, 2.0f * colorBurstCycleCountPerOutputSample); 
        break;

      case FilterType::Simple:
        {
          float colorCycleCount = 1.0f;
          uint32_t windowSize = uint32_t(colorCycleCount * f32(genInfo.outputOversampleAmount) / f32(genInfo.colorCyclesPerInputPixel));
          rollingWindowI.resize(windowSize);
          rollingWindowQ.resize(windowSize);
        }
        
        break;
      }

      // We need some buffers for this processing as well
      u32 outTexelCount = genInfo.inputScanlinePixelCount * genInfo.outputOversampleAmount;
      qData.resize(Math::AlignInt(outTexelCount, k_maxFloatAlignment));
      iData.resize(Math::AlignInt(outTexelCount, k_maxFloatAlignment));
      rgbs.resize(Math::AlignInt(outTexelCount * 3, outTexelCount * 3));
      scratch.resize(Math::AlignInt(outTexelCount * 3, k_maxFloatAlignment));
    }

    void DecodeScanlineToARGB(
      const Context &context,
      const AlignedVector<f32> &lumaSignal,
      const AlignedVector<f32> &chromaSignal,
      f32 hue,
      f32 saturation,
      f32 sharpness,
      u32 *rgbOut)
    {
      if (hue != lastHue)
      {
        // We need to store the cos/sin of the decode hue (so we don't calculate them every pixel/scanline)
        lastHue = hue;
        cosHue = Math::CosPi(hue);
        sinHue = Math::SinPi(hue);
      }
      if (filterType == FilterType::Simple)
      {
        f32 iRunningSum = 0.0f;
        f32 qRunningSum = 0.0f;
        memset(rollingWindowI.data(), 0, rollingWindowI.size() * sizeof(32));
        memset(rollingWindowQ.data(), 0, rollingWindowQ.size() * sizeof(32));
        nextRollingWindowIndex = 0;

        for (u32 x = 0; x < context.OutputTexelCount(); x++)
        {
          f32 sin = -(context.Sin(x) * cosHue + context.Cos(x) * sinHue);
          f32 cos =   context.Cos(x) * cosHue - context.Sin(x) * sinHue;

          f32 i = chromaSignal[x] * sin;
          f32 q = chromaSignal[x] * cos;
          iRunningSum -= rollingWindowI[nextRollingWindowIndex];
          iRunningSum += i;
          rollingWindowI[nextRollingWindowIndex] = i;
          qRunningSum -= rollingWindowQ[nextRollingWindowIndex];
          qRunningSum += q;
          rollingWindowQ[nextRollingWindowIndex] = q;

          nextRollingWindowIndex = (nextRollingWindowIndex + 1) % rollingWindowI.size();

          iData[x] = iRunningSum / f32(rollingWindowI.size());
          qData[x] = qRunningSum / f32(rollingWindowQ.size());
        }
      }
      else
      {
        // The color signal, ultimately, is encoded as QAM (Quadrature Amplitude Modulation). There are two waves of data at the same frequency
        //  that are off from each other by 90 degrees (hey, just like cos and sin!). And since we know the phase of the intended wav, we can 
        //  multiplying the chroma wav by each of sin and cos of the chroma signal (off by 90 degrees, in phase with the signal we expect), and
        //  then low-passing that to get the actual q/i data (q and i are then used to decode the color)
        if (filterType == FilterType::IIR)
        {
          demodulateFilterIIR.ResetHistory();
        }

        for (u32 x = 0; x < context.OutputTexelCount(); x++)
        {
          // Thanks to sin/cos identities we can combine our sin/cos tables with our decode hue:
          //  Cos([2*phase + artifactHue] + decodeHue) -> Cos(A + decodeHue) -> Cos(A)*Cos(decodeHue) - Sin(A)*Sin(DecodeHue);
          f32 cos = context.Cos(x) * cosHue - context.Sin(x) * sinHue;
          scratch[x] = chromaSignal[x] * cos;
        }

        switch (filterType)
        {
        case FilterType::IIR:
          demodulateFilterIIR.Process(scratch, context.OutputTexelCount(), &qData);
          break;

        case FilterType::FIR:
          demodulateFilterFIR.Process(scratch, context.OutputTexelCount(), &qData);
          break;
        }

        // Reset history again - the two demodulations are independent from a history standpoint (as are the scanlines)
        if (filterType == FilterType::IIR)
        {
          demodulateFilterIIR.ResetHistory();
        }

        for (u32 x = 0; x < context.OutputTexelCount(); x++)
        {
          // Sin([2*phase + artifactHue] + decodeHue) -> Sin(A + decodeHue) -> Sin(A)*Cos(decodeHue) + Cos(A)*Sin(DecodeHue);
          f32 sin = -(context.Sin(x) * cosHue + context.Cos(x) * sinHue);
          scratch[x] = chromaSignal[x] * sin; // -SinPi(2.0f * phase + k_artifactHue + k_decodeHue);
        }

        switch (filterType)
        {
        case FilterType::IIR:
          demodulateFilterIIR.Process(scratch, context.OutputTexelCount(), &iData);
          break;

        case FilterType::FIR:
          demodulateFilterFIR.Process(scratch, context.OutputTexelCount(), &iData);
          break;
        }
      }

      for (u32 x = 0; x < context.OutputTexelCount(); x++)
      {
        // Decode the Yiq components (and apply saturation)
        f32 Y = lumaSignal[x];
        f32 i = iData[x] * 2.0f * saturation;
        f32 q = qData[x] * 2.0f * saturation;

        // Convert to RGB using a standard Yiq -> rgb matrix
        //f32 r = Y + i * 0.946882f + q * 0.623557f;
        //f32 g = Y - i * 0.274788f - q * 0.635691f;
        //f32 b = Y - i * 1.108545f + q * 1.7090047f;

        rgbs[x * 3 + 0] = Y;
        rgbs[x * 3 + 1] = i * 0.5f + 0.5f;
        rgbs[x * 3 + 2] = q * 0.5f + 0.5f;
      }

      if (sharpness != 0.0f)
      {
        f32 blurSide = -sharpness / 3.0f;
        f32 center = 1.0f - 2.0f * blurSide;
        u32 blurStep = context.GenInfo().outputOversampleAmount;

        std::swap(scratch, rgbs);

        for (u32 x = 0; x < blurStep; x++)
        {
          for (u32 i = 0; i < 3; i++)
          {
            rgbs[x*3 + i] = scratch[x*3 + i] * center + scratch[(x+blurStep)*3 + i] * blurSide;
          }
        }

        for (u32 x = blurStep; x < context.OutputTexelCount() - blurStep; x++)
        {
          for (u32 i = 0; i < 3; i++)
          {
            rgbs[x*3 + i] = scratch[x*3 + i] * center + scratch[(x-blurStep)*3 + i] * blurSide + scratch[(x+blurStep)*3 + i] * blurSide;
          }
        }

        for (u32 x = context.OutputTexelCount() - blurStep; x < context.OutputTexelCount(); x++)
        {
          for (u32 i = 0; i < 3; i++)
          {
            rgbs[x*3 + i] = scratch[x*3 + i] * center + scratch[(x-blurStep)*3 + i] * blurSide;
          }
        }
      }

      for (u32 x = 0; x < context.OutputTexelCount(); x++)
      {
        // Convert to 8-bit and output
        auto r = std::floor(std::clamp(rgbs[x * 3 + 0] * 255.0f, 0.0f, 255.0f));
        auto g = std::floor(std::clamp(rgbs[x * 3 + 1] * 255.0f, 0.0f, 255.0f));
        auto b = std::floor(std::clamp(rgbs[x * 3 + 2] * 255.0f, 0.0f, 255.0f));

        u32 abgr = 0xFF000000 | (u32(r)) | (u32(g) << 8) | (u32(b) << 16);
        rgbOut[x] = abgr;
      }
    }

  private:
    ButterworthIIR::Filter demodulateFilterIIR;
    FIRFilter demodulateFilterFIR;
    FilterType filterType;

    AlignedVector<f32> qData;
    AlignedVector<f32> iData;
    AlignedVector<f32> scratch;
    AlignedVector<f32> rgbs;
    f32 lastHue = 0.0f;
    f32 cosHue = 1.0f;
    f32 sinHue = 0.0f;

    std::vector<f32> rollingWindowI;
    std::vector<f32> rollingWindowQ;
    int nextRollingWindowIndex = 0;
  };
}