#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "GraphicsDevice.h"
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
      float colorCyclesPerInputPixelIn,
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    , colorCyclesPerInputPixel(colorCyclesPerInputPixelIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreatePixelShader(IDR_FILTER_RGB, &blurRGBShader);
    }


    [[nodiscard]] bool Apply(GraphicsDevice *device, ITexture *input, ITexture *output, const TVKnobSettings &knobSettings)
    {
      // We don't have to do anything if the sharpness is 0
      if (knobSettings.sharpness != 0)
      {
        ConstantData data = 
        { 
          -knobSettings.sharpness, 
          colorCyclesPerInputPixel * float(SignalGeneration::k_signalSamplesPerColorCycle) 
        };

        device->DiscardAndUpdateBuffer(constantBuffer, &data);

        device->RenderQuadWithPixelShader(
          blurRGBShader,
          output,
          {input},
          {SamplerType::Clamp},
          {constantBuffer});

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
  
    ComPtr<ID3D11PixelShader> blurRGBShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}