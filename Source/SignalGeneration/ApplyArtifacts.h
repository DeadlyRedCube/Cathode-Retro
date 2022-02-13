#pragma once

#include <algorithm>

#include "SignalGeneration/ArtifactSettings.h"
#include "Constants.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"

namespace NTSCify::SignalGeneration
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


    void Apply(GraphicsDevice *device, ProcessContext *processContext, const ArtifactSettings &options)
    {
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

      processContext->whiteLevel *= (1.0f + options.ghostVisibility);
      processContext->blackLevel *= (1.0f + options.ghostVisibility);

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      std::unique_ptr<ITexture> *source;
      std::unique_ptr<ITexture> *target;

      if (processContext->hasDoubledSignal)
      {
        // If we have a doubled signal, then we either need a four-component texture (if we're writing out (luma, chroma) * 2) or a two-component 
        //  (for composite * 2)
        source = (processContext->signalType == SignalType::SVideo) ? &processContext->fourComponentTex : &processContext->twoComponentTex;
        target = (processContext->signalType == SignalType::SVideo) ? &processContext->fourComponentTexScratch : &processContext->twoComponentTexScratch;
      }
      else
      {
        // The signal is NOT doubled, so we only need a float2 texture (for (luma, chroma)) or a float texture (for composite)
        source = (processContext->signalType == SignalType::SVideo) ? &processContext->twoComponentTex : &processContext->oneComponentTex;
        target = (processContext->signalType == SignalType::SVideo) ? &processContext->twoComponentTexScratch : &processContext->oneComponentTexScratch;
      }
      
      device->RenderQuadWithPixelShader(
        applyArtifactsShader,
        target->get(),
        {source->get()},
        {SamplerType::Clamp},
        {constantBuffer});
      std::swap(*source, *target);

      noiseSeed = (noiseSeed + 1) % (60*60);
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
