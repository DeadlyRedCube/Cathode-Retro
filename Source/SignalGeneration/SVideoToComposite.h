#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalGeneration
{
  class SVideoToComposite
  {
  public:
    SVideoToComposite(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateComputeShader(IDR_SVIDEO_TO_COMPOSITE, &sVideoToCompositeShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers)
    {
      auto context = device->Context();

      auto srv = buffers->signalSRVTwoComponentA.Ptr();
      auto uav = buffers->signalUAVOneComponentA.Ptr();

      context->CSSetShader(sVideoToCompositeShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
    }

  private:
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> sVideoToCompositeShader;
  };
}