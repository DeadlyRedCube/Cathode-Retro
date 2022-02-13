#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  // This takes a YIQ input texture and converts its color space to be RGB.
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


    void Apply(GraphicsDevice *device, ProcessContext *processContext, const TVKnobSettings &knobSettings)
    {
      ConstantData data = { knobSettings.gamma };
      device->DiscardAndUpdateBuffer(constantBuffer, &data);

      processContext->RenderWithComputeShader(
        device,
        yiqToRGBShader,
        processContext->colorTex.texture,
        processContext->colorTex.uav,
        {processContext->fourComponentTex.srv},
        {processContext->samplerStateClamp},
        {constantBuffer});
    }

  private:
    struct ConstantData
    {
      float gamma;        // The gamma to apply ($TODO I think this should go away)
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    ComPtr<ID3D11ComputeShader> yiqToRGBShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}