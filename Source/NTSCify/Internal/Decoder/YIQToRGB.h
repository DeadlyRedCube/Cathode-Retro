#pragma once

#include <algorithm>

#include "NTSCify/Internal/Util.h"
#include "NTSCify/GraphicsDevice.h"
#include "NTSCify/TVKnobSettings.h"


namespace NTSCify::Internal::Decoder
{
  // This takes a YIQ input texture and converts its color space to be RGB.
  class YIQToRGB
  {
  public:
    YIQToRGB(
      IGraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      yiqToRGBShader = device->CreateShader(ShaderID::YIQToRGB);
    }


    void Apply(IGraphicsDevice *device, const ITexture *inputYIQ, ITexture *outputRGB)
    {
      device->RenderQuad(
        yiqToRGBShader.get(),
        outputRGB,
        {inputYIQ},
        {SamplerType::LinearClamp},
        {});
    }

  private:
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    std::unique_ptr<IShader> yiqToRGBShader;
  };
}