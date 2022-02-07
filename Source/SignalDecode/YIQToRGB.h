#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  class YIQToRGB
  {
  public:
    YIQToRGB(
      GraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);

      device->CreateComputeShader(IDR_YIQ_TO_RGB, &yiqToRGBShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers, const TVKnobSettings &knobSettings)
    {
      auto context = device->Context();
      ConstantData data = { knobSettings.gamma };
      device->DiscardAndUpdateBuffer(constantBuffer, &data);

      auto srv = buffers->signalSRVFourComponentA.Ptr();
      auto uav = buffers->signalUAVColorA.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(yiqToRGBShader, nullptr, 0);
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
      float gamma;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    ComPtr<ID3D11ComputeShader> yiqToRGBShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}