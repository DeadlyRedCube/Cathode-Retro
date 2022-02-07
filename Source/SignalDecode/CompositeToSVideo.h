#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "SignalGeneration/Constants.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
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


    void Apply(GraphicsDevice *device, ProcessContext *buffers)
    {
      if (buffers->signalType == SignalGeneration::SignalType::SVideo)
      {
        return;
      }

      auto context = device->Context();

      ConstantData cd = { SignalGeneration::k_signalSamplesPerColorCycle };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);


      auto srv = buffers->hasDoubledSignal ? buffers->twoComponentTex.srv.Ptr() : buffers->oneComponentTex.srv.Ptr();
      auto uav = buffers->hasDoubledSignal ? buffers->fourComponentTex.uav.Ptr() :  buffers->twoComponentTex.uav.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(compositeToSVideoShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);


      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> compositeToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}