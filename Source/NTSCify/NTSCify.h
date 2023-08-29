#pragma once

#include <memory>

#include "NTSCify/Internal/CRT/RGBToCRT.h"
#include "NTSCify/Internal/Decoder/SignalDecoder.h"
#include "NTSCify/Internal/Generator/SignalGenerator.h"
#include "NTSCify/GraphicsDevice.h"
#include "NTSCify/ScanlineTYpe.h"


namespace NTSCify
{
  class NTSCify
  {
  public:
    NTSCify(IGraphicsDevice *graphicsDevice,
      SignalType sigType,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const SourceSettings &sourceSettings,
      const ArtifactSettings &artifactSettings,
      const TVKnobSettings &knobSettings,
      const OverscanSettings &overscanSettings,
      const ScreenSettings &screenSettings)
      : device(graphicsDevice)
      , signalType(sigType)
      , cachedSourceSettings(sourceSettings)
      , inWidth(inputWidth)
      , inHeight(inputHeight)
    {
      using namespace Internal;

      signalGenerator = std::make_unique<Generator::SignalGenerator>(
        device,
        signalType,
        inputWidth,
        inputHeight,
        sourceSettings);

      signalGenerator->SetArtifactSettings(artifactSettings);

      signalDecoder = std::make_unique<Decoder::SignalDecoder>(
        device,
        signalGenerator->SignalProperties());

      signalDecoder->SetKnobSettings(knobSettings);

      rgbToCRT = std::make_unique<CRT::RGBToCRT>(
        device,
        inputWidth,
        signalGenerator->SignalProperties().scanlineWidth,
        inputHeight,
        signalGenerator->SignalProperties().inputPixelAspectRatio);

      rgbToCRT->SetOverscanSettings(overscanSettings);
      rgbToCRT->SetScreenSettings(screenSettings);
    }


    void UpdateSettings(
      SignalType sigType,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const SourceSettings &sourceSettings,
      const ArtifactSettings &artifactSettings,
      const TVKnobSettings &knobSettings,
      const OverscanSettings &overscanSettings,
      const ScreenSettings &screenSettings)
    {
      using namespace Internal;

      auto oldSignalProps = signalGenerator->SignalProperties();
      bool rebuildGen = false;
      bool rebuildCRT = false;
      if (inputWidth != inWidth || inputHeight != inHeight)
      {
        rebuildGen = true;
        rebuildCRT = true;
      }

      if (rebuildGen || signalType != sigType || sourceSettings != cachedSourceSettings)
      {
        signalType = sigType;
        cachedSourceSettings = sourceSettings;
        inWidth = inputWidth;
        inHeight = inputHeight;

        signalGenerator = std::make_unique<Generator::SignalGenerator>(
          device,
          signalType,
          inputWidth,
          inputHeight,
          sourceSettings);
      }

      signalGenerator->SetArtifactSettings(artifactSettings);

      if (signalGenerator->SignalProperties() != oldSignalProps)
      {
        signalDecoder = std::make_unique<Decoder::SignalDecoder>(
          device,
          signalGenerator->SignalProperties());
      }

      signalDecoder->SetKnobSettings(knobSettings);

      if (rebuildCRT
        || signalGenerator->SignalProperties().scanlineWidth != oldSignalProps.scanlineWidth
        || signalGenerator->SignalProperties().inputPixelAspectRatio != oldSignalProps.inputPixelAspectRatio)
      {
        rgbToCRT = std::make_unique<CRT::RGBToCRT>(
          device,
          inputWidth,
          signalGenerator->SignalProperties().scanlineWidth,
          inputHeight,
          signalGenerator->SignalProperties().inputPixelAspectRatio);
      }

      rgbToCRT->SetOverscanSettings(overscanSettings);
      rgbToCRT->SetScreenSettings(screenSettings);
    }


    void Render(
      const ITexture *currentFrameInputRGB,
      const ITexture *previousFrameInputRGB,
      ScanlineType scanlineType,
      ITexture *output)
    {
      if (signalType != SignalType::RGB)
      {
        signalGenerator->Generate(currentFrameInputRGB);
        signalDecoder->Decode(
          signalGenerator->SignalTexture(),
          signalGenerator->PhasesTexture(),
          signalGenerator->SignalLevels());

        currentFrameInputRGB = signalDecoder->CurrentFrameRGBOutput();
        previousFrameInputRGB = signalDecoder->PreviousFrameRGBOutput();
      }

      rgbToCRT->Render(
        currentFrameInputRGB,
        previousFrameInputRGB,
        output,
        scanlineType);
    }

  private:
    IGraphicsDevice *device;
    SignalType signalType;
    SourceSettings cachedSourceSettings;
    uint32_t inWidth;
    uint32_t inHeight;

    std::unique_ptr<Internal::Generator::SignalGenerator> signalGenerator;
    std::unique_ptr<Internal::Decoder::SignalDecoder> signalDecoder;
    std::unique_ptr<Internal::CRT::RGBToCRT> rgbToCRT;
  };
};