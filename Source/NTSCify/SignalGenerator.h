#pragma once

#include "NTSCify/Constants.h"
#include "NTSCify/SignalLevels.h"
#include "NTSCify/SignalProperties.h"
#include "NTSCify/SourceSettings.h"
#include "NTSCify/GeneratorComponents/ApplyArtifacts.h"
#include "NTSCify/GeneratorComponents/RGBToSVideoOrComposite.h"

namespace NTSCify
{
  class SignalGenerator
  {
  public:
    SignalGenerator(
      GraphicsDevice *deviceIn,
      SignalType type,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const SourceSettings &inputSettings)
    : device(deviceIn)
    {
      sourceSettings = inputSettings;

      signalProps.type = type;
      signalProps.scanlineWidth = int32_t(std::ceil(float(inputWidth * inputSettings.colorCyclesPerInputPixel * k_signalSamplesPerColorCycle) / float(inputSettings.denominator)));
      signalProps.scanlineCount = inputHeight;
      signalProps.colorCyclesPerInputPixel = float(inputSettings.colorCyclesPerInputPixel) / float(inputSettings.denominator);
      signalProps.inputPixelAspectRatio = inputSettings.inputPixelAspectRatio;
      
      rgbToSVideoOrComposite = std::make_unique<GeneratorComponents::RGBToSVideoOrComposite>(device, inputWidth, signalProps.scanlineWidth, signalProps.scanlineCount);
      applyArtifacts = std::make_unique<GeneratorComponents::ApplyArtifacts>(device, signalProps.scanlineWidth, signalProps.scanlineCount);

      phasesTextureSingle = device->CreateTexture(signalProps.scanlineCount, 1, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);
      phasesTextureDoubled = device->CreateTexture(signalProps.scanlineCount, 1, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
    
      switch (type)
      {
      case SignalType::SVideo:
        signalTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
        scratchSignalTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
        signalTextureDoubled = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);
        scratchSignalTextureDoubled = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);
        break;
      
      case SignalType::Composite:
        signalTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);
        scratchSignalTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);
        signalTextureDoubled = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
        scratchSignalTextureDoubled = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
        break;
      }
    }

    const SignalProperties &SignalProperties() const
      { return signalProps; }

    const SignalLevels &SignalLevels() const
      { return levels; }

    const ITexture *PhasesTexture() const 
      { return levels.isDoubled ? phasesTextureDoubled.get() : phasesTextureSingle.get(); }

    const ITexture *SignalTexture() const
      { return levels.isDoubled ? signalTextureDoubled.get() : signalTextureSingle.get(); }

    void SetArtifactSettings(const ArtifactSettings &settings)
      { artifactSettings = settings; }

    void Generate(const ITexture *inputRGBTexture, int32_t frameStartPhaseNumeratorIn = -1)
    {
      if (frameStartPhaseNumeratorIn >= 0)
      {
        frameStartPhaseNumerator = uint32_t(frameStartPhaseNumeratorIn);
      }

      ITexture *scanlinePhase;
      std::unique_ptr<ITexture> *scratch;
      std::unique_ptr<ITexture> *signal;
      if (artifactSettings.temporalArtifactReduction > 0.0f && frameStartPhaseNumerator != prevFrameStartPhaseNumerator)
      {
        levels.isDoubled = true;
        scanlinePhase = phasesTextureDoubled.get();
        scratch = &scratchSignalTextureDoubled;
        signal = &signalTextureDoubled;
        levels.temporalArtifactReduction = artifactSettings.temporalArtifactReduction;
      }
      else
      {
        levels.isDoubled = false;
        scanlinePhase = phasesTextureSingle.get();
        scratch = &scratchSignalTextureSingle;
        signal = &signalTextureSingle;
        levels.temporalArtifactReduction = 0.0f;
      }


      rgbToSVideoOrComposite->Generate(
        device,
        signalProps.type,
        inputRGBTexture,
        scanlinePhase,
        signal->get(),
        &levels,
        float(frameStartPhaseNumerator) / float(sourceSettings.denominator),
        float(prevFrameStartPhaseNumerator) / float(sourceSettings.denominator),
        float(sourceSettings.phaseIncrementPerLine) / float(sourceSettings.denominator),
        artifactSettings);

      if (applyArtifacts->Apply(
        device,
        signal->get(),
        scratch->get(),
        &levels,
        artifactSettings))
      {
        // We applied artifacts so scratch is our new signal, so swap them.
        std::swap(*signal, *scratch);
      }

      isEvenFrame = !isEvenFrame;
      prevFrameStartPhaseNumerator = frameStartPhaseNumerator;
      frameStartPhaseNumerator = (frameStartPhaseNumerator + (isEvenFrame ? sourceSettings.phaseIncrementPerEvenFrame : sourceSettings.phaseIncrementPerOddFrame)) % sourceSettings.denominator;
    }

  private:
    GraphicsDevice *device;

    std::unique_ptr<GeneratorComponents::RGBToSVideoOrComposite> rgbToSVideoOrComposite;
    std::unique_ptr<GeneratorComponents::ApplyArtifacts> applyArtifacts;
    std::unique_ptr<ITexture> phasesTextureSingle;
    std::unique_ptr<ITexture> phasesTextureDoubled;
    
    std::unique_ptr<ITexture> signalTextureSingle;
    std::unique_ptr<ITexture> scratchSignalTextureSingle;
    std::unique_ptr<ITexture> signalTextureDoubled;
    std::unique_ptr<ITexture> scratchSignalTextureDoubled;

    SourceSettings sourceSettings;
    NTSCify::SignalProperties signalProps;
    NTSCify::SignalLevels levels;

    ArtifactSettings artifactSettings;
    uint32_t frameStartPhaseNumerator = 0;
    uint32_t prevFrameStartPhaseNumerator = 0;
    bool isEvenFrame = false;
  };
}