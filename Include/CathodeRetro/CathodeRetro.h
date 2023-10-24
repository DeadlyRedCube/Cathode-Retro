#pragma once

#include <memory>

#include "CathodeRetro/Internal/RGBToCRT.h"
#include "CathodeRetro/Internal/SignalDecoder.h"
#include "CathodeRetro/Internal/SignalGenerator.h"
#include "CathodeRetro/GraphicsDevice.h"
#include "CathodeRetro/Settings.h"


namespace CathodeRetro
{
  // This class handles the whole CathodeRetro pipeline. Besides the constructor, it has three functions:
  //  - UpdateSettings, which should be called whenever any of the overall settings for the rendering are changed
  //  - SetOutputSize, which should be called when the output render target size changes (i.e. the output window resizes)
  //  - Render, which use the supplied IGraphicsDevice to do the magic.
  class CathodeRetro
  {
  public:
    CathodeRetro(
      IGraphicsDevice *graphicsDevice,
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

      signalGenerator = std::make_unique<SignalGenerator>(
        device,
        signalType,
        inputWidth,
        inputHeight,
        sourceSettings);

      signalGenerator->SetArtifactSettings(artifactSettings);

      signalDecoder = std::make_unique<SignalDecoder>(
        device,
        signalGenerator->SignalProperties());

      signalDecoder->SetKnobSettings(knobSettings);

      rgbToCRT = std::make_unique<RGBToCRT>(
        device,
        inputWidth,
        signalGenerator->SignalProperties().scanlineWidth,
        inputHeight,
        signalGenerator->SignalProperties().inputPixelAspectRatio,
        overscanSettings,
        screenSettings);
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

        signalGenerator = std::make_unique<SignalGenerator>(
          device,
          signalType,
          inputWidth,
          inputHeight,
          sourceSettings);
      }

      signalGenerator->SetArtifactSettings(artifactSettings);

      if (signalGenerator->SignalProperties() != oldSignalProps)
      {
        signalDecoder = std::make_unique<SignalDecoder>(
          device,
          signalGenerator->SignalProperties());
      }

      signalDecoder->SetKnobSettings(knobSettings);

      if (rebuildCRT
        || signalGenerator->SignalProperties().scanlineWidth != oldSignalProps.scanlineWidth
        || signalGenerator->SignalProperties().inputPixelAspectRatio != oldSignalProps.inputPixelAspectRatio)
      {
        rgbToCRT = std::make_unique<RGBToCRT>(
          device,
          inputWidth,
          signalGenerator->SignalProperties().scanlineWidth,
          inputHeight,
          signalGenerator->SignalProperties().inputPixelAspectRatio,
          overscanSettings,
          screenSettings);
      }
      else
      {
        rgbToCRT->SetSettings(overscanSettings, screenSettings);
      }
    }


    void SetOutputSize(uint32_t outputWidth, uint32_t outputHeight)
    {
      rgbToCRT->SetOutputSize(outputWidth, outputHeight);
    }


    void Render(
      const ITexture *currentFrameInputRGB,
      const ITexture *previousFrameInputRGB,
      ScanlineType scanlineType,
      ITexture *output)
    {
      device->BeginRendering();

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

      device->EndRendering();
    }

  private:
    IGraphicsDevice *device;
    SignalType signalType;
    SourceSettings cachedSourceSettings;
    uint32_t inWidth;
    uint32_t inHeight;

    std::unique_ptr<Internal::SignalGenerator> signalGenerator;
    std::unique_ptr<Internal::SignalDecoder> signalDecoder;
    std::unique_ptr<Internal::RGBToCRT> rgbToCRT;
  };
};