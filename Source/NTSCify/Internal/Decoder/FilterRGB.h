#pragma once

#include <algorithm>

#include "NTSCify/GraphicsDevice.h"
#include "NTSCify/TVKnobSettings.h"


namespace NTSCify::Internal::Decoder
{
  // Apply a blur or sharpen filter to an RGB texture
  class FilterRGB
  {
  public:
    FilterRGB(
      IGraphicsDevice *device,
      float colorCyclesPerInputPixelIn,
      uint32_t signalTextureWidthIn,
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    , colorCyclesPerInputPixel(colorCyclesPerInputPixelIn)
    {
      constantBuffer = device->CreateConstantBuffer(sizeof(ConstantData));
      blurRGBShader = device->CreateShader(ShaderID::FilterRGB);
    }


    [[nodiscard]] bool Apply(IGraphicsDevice *device, ITexture *input, ITexture *output, const TVKnobSettings &knobSettings)
    {
      // We don't have to do anything if the sharpness is 0
      if (knobSettings.sharpness != 0)
      {
        device->UpdateConstantBuffer(
          constantBuffer.get(),
          ConstantData {
            .blurStrength = -knobSettings.sharpness,
            .blurSampleStepSize = colorCyclesPerInputPixel * float(k_signalSamplesPerColorCycle)
          });

        device->RenderQuad(
          blurRGBShader.get(),
          output,
          {input},
          {SamplerType::LinearClamp},
          {constantBuffer.get()});

        return true;
      }

      return false;
    }

  private:
    struct ConstantData
    {
      float blurStrength;
      float blurSampleStepSize;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    float colorCyclesPerInputPixel;

    std::unique_ptr<IShader> blurRGBShader;
    std::unique_ptr<IConstantBuffer> constantBuffer;
  };
}