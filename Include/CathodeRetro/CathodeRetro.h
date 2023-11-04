#pragma once

#include <memory>

#include "CathodeRetro/Internal/RGBToCRT.h"
#include "CathodeRetro/Internal/SignalDecoder.h"
#include "CathodeRetro/Internal/SignalGenerator.h"
#include "CathodeRetro/GraphicsDevice.h"
#include "CathodeRetro/Settings.h"


namespace CathodeRetro
{
  // This class handles the whole CathodeRetro pipeline.
  class CathodeRetro
  {
  public:
    CathodeRetro(
      IGraphicsDevice *graphicsDevice,
      SignalType sigType,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const SourceSettings &sourceSettings)
      : device(graphicsDevice)
    {
      UpdateSourceSettings(sigType, inputWidth, inputHeight, sourceSettings);
    }

    // Call this whenever the input signal type changes (signal type, timings, or input dimensions). These changes
    //  requires internal textures and other objects to be potentially reallocated.
    void UpdateSourceSettings(
      SignalType sigType,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const SourceSettings &sourceSettings)
    {
      if (rgbToCRT != nullptr
        && inputWidth == inWidth
        && inputHeight == inHeight
        && sigType == signalType
        && sourceSettings == cachedSourceSettings)
      {
        return;
      }

      using namespace Internal;

      signalType = sigType;
      cachedSourceSettings = sourceSettings;
      inWidth = inputWidth;
      inHeight = inputHeight;

      if (sigType == SignalType::RGB)
      {
        signalGenerator = nullptr;
        signalDecoder = nullptr;
        rgbToCRT = std::make_unique<RGBToCRT>(
          device,
          inputWidth,
          inputWidth,
          inputHeight,
          sourceSettings.inputPixelAspectRatio);
      }
      else
      {
        signalGenerator = std::make_unique<SignalGenerator>(
          device,
          signalType,
          inputWidth,
          inputHeight,
          sourceSettings);
        signalGenerator->SetArtifactSettings(cachedArtifactSettings);

        signalDecoder = std::make_unique<SignalDecoder>(device, signalGenerator->SignalProperties());
        signalDecoder->SetKnobSettings(cachedKnobSettings);

        rgbToCRT = std::make_unique<RGBToCRT>(
          device,
          inputWidth,
          signalDecoder->OutputTextureWidth(),
          inputHeight,
          signalGenerator->SignalProperties().inputPixelAspectRatio);
      }

      if (outWidth != 0 && outHeight != 0)
      {
        rgbToCRT->SetOutputSize(outWidth, outHeight);
      }

      rgbToCRT->SetSettings(cachedOverscanSettings, cachedScreenSettings);
    }


    // Call this to change any other settings. No reallocations or texture re-creations are necessary here.
    void UpdateSettings(
      const ArtifactSettings &artifactSettings,
      const TVKnobSettings &knobSettings,
      const OverscanSettings &overscanSettings,
      const ScreenSettings &screenSettings)
    {
      using namespace Internal;

      cachedArtifactSettings = artifactSettings;
      cachedKnobSettings = knobSettings;
      cachedOverscanSettings = overscanSettings;
      cachedScreenSettings = screenSettings;

      if (signalGenerator != nullptr)
      {
        signalGenerator->SetArtifactSettings(artifactSettings);
      }

      if (signalDecoder != nullptr)
      {
        signalDecoder->SetKnobSettings(knobSettings);
      }

      if (rgbToCRT != nullptr)
      {
        rgbToCRT->SetSettings(overscanSettings, screenSettings);
      }
    }


    // Call this to change the output size (i.e. the size of the texture we'll be rendering to). This will reallocate
    //  any screen-sized textures that might exist.
    void SetOutputSize(uint32_t outputWidth, uint32_t outputHeight)
    {
      assert(outputWidth > 0 && outputHeight > 0);

      if (outputWidth == outWidth && outputHeight == outHeight)
      {
        return;
      }

      outWidth = outputWidth;
      outHeight = outputHeight;

      if (rgbToCRT != nullptr)
      {
        rgbToCRT->SetOutputSize(outputWidth, outputHeight);
      }
    }


    // Call this to actually render
    void Render(
      const ITexture *currentFrameInputRGB,
      ScanlineType scanlineType,
      IRenderTarget *output)
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
      }

      rgbToCRT->Render(
        currentFrameInputRGB,
        output,
        scanlineType);

      device->EndRendering();
    }

  private:
    IGraphicsDevice *device;
    SignalType signalType;
    SourceSettings cachedSourceSettings;
    ArtifactSettings cachedArtifactSettings;
    TVKnobSettings cachedKnobSettings;
    OverscanSettings cachedOverscanSettings;
    ScreenSettings cachedScreenSettings;

    uint32_t inWidth = 0;
    uint32_t inHeight = 0;
    uint32_t outWidth = 0;
    uint32_t outHeight = 0;

    std::unique_ptr<Internal::SignalGenerator> signalGenerator;
    std::unique_ptr<Internal::SignalDecoder> signalDecoder;
    std::unique_ptr<Internal::RGBToCRT> rgbToCRT;
  };
};