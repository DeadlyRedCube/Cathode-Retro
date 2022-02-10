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
      device->CreateComputeShader(IDR_COMPOSITE_TO_SVIDEO, &compositeToSVideoShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *processContext)
    {
      if (processContext->signalType == SignalGeneration::SignalType::SVideo)
      {
        // We're actually already using SVideo so there's nothing to do here.
        return;
      }

      auto deviceContext = device->Context();

      ConstantData cd = { SignalGeneration::k_signalSamplesPerColorCycle };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      auto srv = processContext->hasDoubledSignal ? processContext->twoComponentTex.srv.Ptr() : processContext->oneComponentTex.srv.Ptr();
      auto uav = processContext->hasDoubledSignal ? processContext->fourComponentTex.uav.Ptr() :  processContext->twoComponentTex.uav.Ptr();
      auto cb = constantBuffer.Ptr();

      deviceContext->CSSetShader(compositeToSVideoShader, nullptr, 0);
      deviceContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      deviceContext->CSSetConstantBuffers(0, 1, &cb);
      deviceContext->CSSetShaderResources(0, 1, &srv);

      deviceContext->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      deviceContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      deviceContext->CSSetShaderResources(0, 1, &srv);
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match SignalGeneration::k_signalSamplesPerColorCycle
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> compositeToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}