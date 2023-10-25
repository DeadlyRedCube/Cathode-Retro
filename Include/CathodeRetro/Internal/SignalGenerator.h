#pragma once

#include "CathodeRetro/Internal/Constants.h"
#include "CathodeRetro/Internal/SignalLevels.h"
#include "CathodeRetro/Internal/SignalProperties.h"
#include "CathodeRetro/Settings.h"

namespace CathodeRetro
{
  namespace Internal
  {
    class SignalGenerator
    {
    public:
      SignalGenerator(
        IGraphicsDevice *deviceIn,
        SignalType type,
        uint32_t inputWidth,
        uint32_t inputHeight,
        const SourceSettings &inputSettings)
      : device(deviceIn)
      {
        sourceSettings = inputSettings;

        signalProps.type = type;
        signalProps.scanlineWidth = int32_t(std::ceil(
          float(inputWidth * inputSettings.colorCyclesPerInputPixel * k_signalSamplesPerColorCycle) / float(inputSettings.denominator)));
        signalProps.scanlineCount = inputHeight;
        signalProps.colorCyclesPerInputPixel = float(inputSettings.colorCyclesPerInputPixel) / float(inputSettings.denominator);
        signalProps.inputPixelAspectRatio = inputSettings.inputPixelAspectRatio;

        generateSignalConstantBuffer = device->CreateConstantBuffer(
          std::max(sizeof(RGBToSVideoConstantData), sizeof(GeneratePhaseTextureConstantData)));
        rgbToSVideoShader = device->CreateShader(ShaderID::RGBToSVideoOrComposite);
        generatePhaseTextureShader = device->CreateShader(ShaderID::GeneratePhaseTexture);

        applyArtifactsConstantBuffer = device->CreateConstantBuffer(sizeof(ApplyArtifactsConstantData));
        applyArtifactsShader = device->CreateShader(ShaderID::ApplyArtifacts);

        SetArtifactSettings({});
      }

      const Internal::SignalProperties &SignalProperties() const
        { return signalProps; }

      const Internal::SignalLevels &SignalLevels() const
        { return levels; }

      const ITexture *PhasesTexture() const
        { return phasesTexture.get(); }

      const ITexture *SignalTexture() const
        { return signalTexture.get(); }

      void SetArtifactSettings(const ArtifactSettings &settings)
      {
        artifactSettings = settings;

        // If we have any temporal artifact reduction we are going to double up our generated signal textures so that two phases of the same
        //  frame can be blended together by the decoder.
        bool wantsDouble = (artifactSettings.temporalArtifactReduction > 0.0f);

        TextureFormat phasesFormat = wantsDouble ? TextureFormat::RG_Float32 : TextureFormat::R_Float32;
        TextureFormat signalFormat = wantsDouble
          ? ((signalProps.type == SignalType::SVideo) ? TextureFormat::RGBA_Float32 : TextureFormat::RG_Float32)
          : ((signalProps.type == SignalType::SVideo) ? TextureFormat::RG_Float32 : TextureFormat::R_Float32);

        if (phasesTexture == nullptr || phasesTexture->Format() != phasesFormat)
        {
          phasesTexture = device->CreateRenderTarget(signalProps.scanlineCount, 1, 1, phasesFormat);
        }

        if (signalTexture == nullptr || signalTexture->Format() != signalFormat)
        {
          signalTexture = device->CreateRenderTarget(signalProps.scanlineWidth, signalProps.scanlineCount, 1, signalFormat);
          scratchSignalTexture = device->CreateRenderTarget(signalProps.scanlineWidth, signalProps.scanlineCount, 1, signalFormat);
        }
      }

      void Generate(const ITexture *inputRGBTexture, int32_t frameStartPhaseNumeratorIn = -1)
      {
        if (frameStartPhaseNumeratorIn >= 0)
        {
          frameStartPhaseNumerator = uint32_t(frameStartPhaseNumeratorIn);
        }

        GeneratePhasesTexture();
        GenerateCleanSignal(inputRGBTexture);

        if (artifactSettings.noiseStrength > 0.0f || artifactSettings.ghostVisibility > 0.0f)
        {
          // Apply artifacts to the signal, then swap signal and scratch to get the artifact output
          ApplyArtifacts();
        }

        isEvenFrame = !isEvenFrame;
        prevFrameStartPhaseNumerator = frameStartPhaseNumerator;
        frameStartPhaseNumerator = (frameStartPhaseNumerator
            + (isEvenFrame ? sourceSettings.phaseIncrementPerEvenFrame : sourceSettings.phaseIncrementPerOddFrame))
          % sourceSettings.denominator;
        noiseSeed = (noiseSeed + 1) & 0x000FFFFF;
      }

    private:
      struct RGBToSVideoConstantData
      {
        uint32_t outputTexelsPerColorburstCycle;        // This value should match k_signalSamplesPerColorCycle
        uint32_t inputWidth;                            // The width of the input, in texels
        uint32_t outputWidth;                           // The width of the output, in texels
        uint32_t scanlineCount;                         // How many scanlines
        float compositeBlend;                           // 0 if we're outputting to SVideo, 1 if it's composite
        float instabilityScale;
        uint32_t noiseSeed;
      };

      struct GeneratePhaseTextureConstantData
      {
        float initialFrameStartPhase;                   // The phase at the start of the first scanline of this frame
        float prevFrameStartPhase;                      // The phase at the start of the previous scanline of this frame (if relevant)
        float phaseIncrementPerScanline;                // The amount to increment the phase each scanline
        uint32_t samplesPerColorburstCycle;             // Should match k_signalSamplesPerColorCycle
        float instabilityScale;
        uint32_t noiseSeed;
        uint32_t signalTextureWidth;
        uint32_t scanlineCount;                         // How many scanlines
      };

      struct ApplyArtifactsConstantData
      {
        // These parameters affect the ghosting.
        float ghostVisibility;
        float ghostDistance;
        float ghostSpreadScale;

        // Noise strength and seed to adjust how much noise there is (And how to animate it)
        float noiseStrength;
        uint32_t noiseSeed;

        // The width of the signal texture, in texels.
        uint32_t signalTextureWidth;

        // How many scanlines there are to the signal
        uint32_t scanlineCount;
        uint32_t samplesPerColorburstCycle;
      };


      void GeneratePhasesTexture()
      {
        // Update our scanline phases texture
        generateSignalConstantBuffer->Update(
          GeneratePhaseTextureConstantData{
            float(frameStartPhaseNumerator) / float(sourceSettings.denominator),
            float(prevFrameStartPhaseNumerator) / float(sourceSettings.denominator),
            float(sourceSettings.phaseIncrementPerLine) / float(sourceSettings.denominator),
            k_signalSamplesPerColorCycle,
            artifactSettings.instabilityScale,
            noiseSeed,
            signalTexture->Width(),
            signalTexture->Height(),
          });

        device->RenderQuad(
          generatePhaseTextureShader.get(),
          phasesTexture.get(),
          {},
          generateSignalConstantBuffer.get());
      }


      void GenerateCleanSignal(const ITexture *rgbTexture)
      {
        // Now run the actual shader
        generateSignalConstantBuffer->Update(
          RGBToSVideoConstantData{
            k_signalSamplesPerColorCycle,
            rgbTexture->Width(),
            signalTexture->Width(),
            signalTexture->Height(),
            (signalProps.type == SignalType::Composite) ? 1.0f : 0.0f,
            artifactSettings.instabilityScale,
            noiseSeed,
          });

        device->RenderQuad(
          rgbToSVideoShader.get(),
          signalTexture.get(),
          {{rgbTexture, SamplerType::LinearClamp}, {phasesTexture.get(), SamplerType::LinearClamp}},
          generateSignalConstantBuffer.get());

        levels.temporalArtifactReduction = artifactSettings.temporalArtifactReduction;
        levels.blackLevel = 0.0f;
        levels.whiteLevel = 1.0f;
        levels.saturationScale = 0.5f;
      }


      void ApplyArtifacts()
      {
        applyArtifactsConstantBuffer->Update(
          ApplyArtifactsConstantData {
            artifactSettings.ghostVisibility,
            artifactSettings.ghostDistance,
            artifactSettings.ghostSpreadScale,

            artifactSettings.noiseStrength,
            noiseSeed,

            scratchSignalTexture->Width(),
            scratchSignalTexture->Height(),
            k_signalSamplesPerColorCycle,
          });

        device->RenderQuad(
          applyArtifactsShader.get(),
          scratchSignalTexture.get(),
          {{signalTexture.get(), SamplerType::LinearClamp}},
          applyArtifactsConstantBuffer.get());

        // Our output is the new signal texture so swap scratch into place.
        std::swap(signalTexture, scratchSignalTexture);
      }


      IGraphicsDevice *device;

      uint32_t noiseSeed = 0;

      std::unique_ptr<IShader> rgbToSVideoShader;
      std::unique_ptr<IShader> generatePhaseTextureShader;
      std::unique_ptr<IShader> applyArtifactsShader;
      std::unique_ptr<IConstantBuffer> generateSignalConstantBuffer;
      std::unique_ptr<IConstantBuffer> applyArtifactsConstantBuffer;

      std::unique_ptr<ITexture> phasesTexture;

      std::unique_ptr<ITexture> signalTexture;
      std::unique_ptr<ITexture> scratchSignalTexture;

      SourceSettings sourceSettings;
      Internal::SignalProperties signalProps;
      Internal::SignalLevels levels;

      ArtifactSettings artifactSettings;

      uint32_t frameStartPhaseNumerator = 0;
      uint32_t prevFrameStartPhaseNumerator = 0;
      bool isEvenFrame = false;
    };
  }
}