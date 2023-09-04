#pragma once

#include <algorithm>

#include "NTSCify/Internal/Constants.h"
#include "NTSCify/Internal/SignalLevels.h"
#include "NTSCify/ArtifactSettings.h"
#include "NTSCify/GraphicsDevice.h"


namespace NTSCify::Internal::Generator
{
  // Apply any ghosting or noise that we want to the input (which might be an SVideo texture or it might be Composite)
  class ApplyArtifacts
  {
  public:
    ApplyArtifacts(IGraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      constantBuffer = device->CreateConstantBuffer(sizeof(ConstantData));
      applyArtifactsShader = device->CreateShader(ShaderID::ApplyArtifacts);
    }


    [[nodiscard]] bool Apply(
      IGraphicsDevice *device,
      const ITexture *inputSignal,
      ITexture *outputSignal,
      SignalLevels *levelsInOut,
      const ArtifactSettings &options)
    {
      if (options.noiseStrength <= 0.0f && options.ghostVisibility <= 0.0f)
      {
        return false;
      }

      // First step: Convert the luma/chroma at signalUAVTwoComponentA into a composite texture at signalUAVOneComponentA
      // These parameters affect the ghosting.
      levelsInOut->whiteLevel *= (1.0f + options.ghostVisibility);
      levelsInOut->blackLevel *= (1.0f + options.ghostVisibility);

      device->UpdateConstantBuffer(
        constantBuffer.get(),
        ConstantData {
          .ghostVisibility = options.ghostVisibility,
          .ghostDistance = options.ghostDistance,
          .ghostSpreadScale = options.ghostSpreadScale,

          .noiseStrength = options.noiseStrength,
          .noiseSeed = noiseSeed,

          .signalTextureWidth = signalTextureWidth,
          .scanlineCount = scanlineCount,
          .samplesPerColorburstCycle = k_signalSamplesPerColorCycle,
        });

      device->RenderQuad(
        applyArtifactsShader.get(),
        outputSignal,
        {inputSignal},
        {SamplerType::LinearClamp},
        {constantBuffer.get()});

      noiseSeed = (noiseSeed + 1) % (60*60);
      return true;
    }

  private:
    struct ConstantData
    {
      // These parameters affect the ghosting.
      float ghostVisibility;
      float ghostDistance;
      float ghostSpreadScale;

      // Noise strength and seed to adjust how much noise there is (And how to animate it)
      float noiseStrength;
      int32_t noiseSeed;

      // The width of the signal texture, in texels.
      uint32_t signalTextureWidth;

      // How many scanlines there are to the signal
      uint32_t scanlineCount;
      uint32_t samplesPerColorburstCycle;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    std::unique_ptr<IShader> applyArtifactsShader;
    std::unique_ptr<IConstantBuffer> constantBuffer;

    int32_t noiseSeed = 0;
  };
}
