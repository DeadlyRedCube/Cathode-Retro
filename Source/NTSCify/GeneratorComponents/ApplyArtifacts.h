#pragma once

#include <algorithm>

#include "NTSCify/ArtifactSettings.h"
#include "NTSCify/Constants.h"
#include "NTSCify/SignalLevels.h"

#include "GraphicsDevice.h"
#include "resource.h"
#include "Util.h"

namespace NTSCify::GeneratorComponents
{
  // Apply any ghosting or noise that we want to the input (which might be an SVideo texture or it might be Composite)
  class ApplyArtifacts
  {
  public:
    ApplyArtifacts(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreatePixelShader(IDR_APPLY_ARTIFACTS, &applyArtifactsShader);
    }


    [[nodiscard]] bool Apply(
      GraphicsDevice *device, 
      const ITexture *inputSignal,
      ITexture *outputSignal,
      SignalLevels *levelsInOut,
      const ArtifactSettings &options)
    {
      if (options.noiseStrength <= 0.0f && options.ghostVisibility != 0.0f)
      {
        return false;
      }

      // First step: Convert the luma/chroma at signalUAVTwoComponentA into a composite texture at signalUAVOneComponentA
      ConstantData cd = 
      {
        options.ghostSpreadScale,
        options.ghostVisibility,
        options.ghostDistance,
        noiseSeed,
        options.noiseStrength,
        signalTextureWidth,
        scanlineCount,
        k_signalSamplesPerColorCycle,
      };

      levelsInOut->whiteLevel *= (1.0f + options.ghostVisibility);
      levelsInOut->blackLevel *= (1.0f + options.ghostVisibility);

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      device->RenderQuadWithPixelShader(
        applyArtifactsShader,
        outputSignal,
        {inputSignal},
        {SamplerType::Clamp},
        {constantBuffer});

      noiseSeed = (noiseSeed + 1) % (60*60);
      return true;
    }

  private:
    struct ConstantData
    {
      // These parameters affect the ghosting. $TODO: Reimplement ghosting, what we have now is bad
      float ghostSpreadScale;
      float ghostBrightness;
      float ghostDistance;

      // Noise strength and seed to adjust how much noise there is (And how to animate it)
      int32_t noiseSeed;
      float noiseStrength;

      // The width of the signal texture, in texels.
      uint32_t signalTextureWidth;

      // How many scanlines there are to the signal
      uint32_t scanlineCount;
      uint32_t samplesPerColorburstCycle;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11PixelShader> applyArtifactsShader;
    ComPtr<ID3D11Buffer> constantBuffer;

    int32_t noiseSeed = 0;
  };
}
