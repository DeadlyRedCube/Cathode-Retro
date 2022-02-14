#pragma once

#include <algorithm>

#include "NTSCify/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::DecodeComponents
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
      yiqToRGBShader = device->CreatePixelShader(IDR_YIQ_TO_RGB);
    }


    void Apply(GraphicsDevice *device, const ITexture *inputYIQ, ITexture *outputRGB)
    {
      device->RenderQuadWithPixelShader(
        yiqToRGBShader,
        outputRGB,
        {inputYIQ},
        {SamplerType::Clamp},
        {});
    }

  private:
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    ComPtr<ID3D11PixelShader> yiqToRGBShader;
  };
}