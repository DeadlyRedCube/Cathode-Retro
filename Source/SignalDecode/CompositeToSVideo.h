#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "SignalGeneration/Constants.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  // Take a composite input and break the luma and chroma back up so that it's "SVideo" (this is the NTSC luma/chroma separation process)
  class CompositeToSVideo
  {
  public:
    CompositeToSVideo(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreatePixelShader(IDR_COMPOSITE_TO_SVIDEO, &compositeToSVideoShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *processContext)
    {
      if (processContext->signalType == SignalGeneration::SignalType::SVideo)
      {
        // We're actually already using SVideo so there's nothing to do here.
        return;
      }

      ConstantData cd = { SignalGeneration::k_signalSamplesPerColorCycle, 1.0f / float(signalTextureWidth), 1.0f / float(scanlineCount) };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      auto &outTex = processContext->hasDoubledSignal ? processContext->fourComponentTex : processContext->twoComponentTex;

      processContext->RenderQuadWithPixelShader(
        device,
        compositeToSVideoShader,
        outTex.texture,
        outTex.rtv,
        { processContext->hasDoubledSignal ? processContext->twoComponentTex.srv : processContext->oneComponentTex.srv },
        { processContext->samplerStateClamp },
        { constantBuffer });
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match SignalGeneration::k_signalSamplesPerColorCycle
      float invInputWidth;
      float invInputHeight;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11PixelShader> compositeToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}