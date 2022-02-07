#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  class FilterRGB
  {
  public:
    FilterRGB(
      GraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_FILTER_RGB, &blurRGBShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers, const TVKnobSettings &knobSettings)
    {
      auto context = device->Context();

      if (knobSettings.sharpness != 0)
      {
        ConstantData data = { -knobSettings.sharpness, SignalGeneration::k_signalSamplesPerColorCycle };
        device->DiscardAndUpdateBuffer(constantBuffer, &data);

        auto srv = buffers->colorTex.srv.Ptr();
        auto uav = buffers->colorTexScratch.uav.Ptr();
        auto cb = constantBuffer.Ptr();

        context->CSSetShader(blurRGBShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);
        context->CSSetShaderResources(0, 1, &srv);

        context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

        srv = nullptr;
        uav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, 1, &srv);

        std::swap(buffers->colorTex, buffers->colorTexScratch);
      }
    }

  private:
    struct ConstantData
    {
      float blurStrength;
      uint32_t texelStepSize;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    ComPtr<ID3D11ComputeShader> blurRGBShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}