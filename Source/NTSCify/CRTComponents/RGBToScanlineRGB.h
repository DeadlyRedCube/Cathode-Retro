#pragma once

#include <algorithm>

#include "NTSCify/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::CRTComponents
{
  // This takes an even or odd interlaced frame and adds the scanlining
  class RGBToScanlineRGB
  {
  public:
    RGBToScanlineRGB(GraphicsDevice *device)
    {
      constantBuffer = device->CreateConstantBuffer(sizeof(ConstantData));
      shader = device->CreatePixelShader(IDR_RGB_TO_SCANLINE_RGB);
    }


    void Apply(GraphicsDevice *device, const ITexture *inputRGB, ITexture *outputRGB, bool isOddFrame, float scanlineStrength)
    {
      ConstantData data = 
      { 
        isOddFrame ? 1u : 0u,
        scanlineStrength,
      };

      device->DiscardAndUpdateBuffer(constantBuffer, &data);
      device->RenderQuadWithPixelShader(
        shader,
        outputRGB,
        {inputRGB},
        {SamplerType::PointClamp},
        {constantBuffer});
    }

  private:
    struct ConstantData
    {
      uint32_t isOddFrame;
      float scanlineStrength;
    };
    ComPtr<ID3D11PixelShader> shader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}