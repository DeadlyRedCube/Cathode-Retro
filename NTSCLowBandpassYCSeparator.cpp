#include "NTSCData.h"
#include "NTSCLowBandpassYCSeparator.h"

namespace NTSC
{
  LowbandpassYCSeparator::LowbandpassYCSeparator(const GenerationInfo &genInfo)
  {
    f32 colorBurstCyclePerOutputSample = f32(genInfo.colorCyclesPerInputPixel) / f32(genInfo.outputOversampleAmount);

    // The luma filter is a simple lowpass filter that cuts everything off above our color frequency
    lumaFilter = FIRFilter::CreateLowPass(colorBurstCyclePerOutputSample);

    // The chroma filter is a bandpass filter, removing the signal above and below the color burst.
    chromaFilter = FIRFilter::Convolve(
      FIRFilter::CreateHighPass(
        colorBurstCyclePerOutputSample,
        2.5f * colorBurstCyclePerOutputSample / k_colorBurstFrequency),
      FIRFilter::CreateLowPass(colorBurstCyclePerOutputSample, 6.0f * colorBurstCyclePerOutputSample / k_colorBurstFrequency));
  }

  void LowbandpassYCSeparator::Separate(
    [[maybe_unused]] const Context &context,
    const AlignedVector<f32> &compositeIn,
    AlignedVector<f32> *lumaSignalOut,
    AlignedVector<f32> *chromaSignalOut)
  {
    // Run each of our two filters to pull the two signals apart
    lumaFilter.Process(compositeIn, context.OutputTexelCount(), lumaSignalOut);
    chromaFilter.Process(compositeIn, context.OutputTexelCount(), chromaSignalOut);
  }
}