#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  // Apply a blur or sharpen filter to an RGB texture
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


    void Apply(GraphicsDevice *device, ProcessContext *processContext, const TVKnobSettings &knobSettings)
    {
      // We don't have to do anything if the sharpness is 0
      if (knobSettings.sharpness != 0)
      {
        ConstantData data = { -knobSettings.sharpness, SignalGeneration::k_signalSamplesPerColorCycle };
        device->DiscardAndUpdateBuffer(constantBuffer, &data);

        processContext->RenderWithComputeShader(
          device,
          blurRGBShader,
          processContext->colorTexScratch.texture,
          processContext->colorTexScratch.uav,
          {processContext->colorTex.srv},
          {processContext->samplerStateClamp},
          {constantBuffer});

        std::swap(processContext->colorTex, processContext->colorTexScratch);
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