#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "NTSCify/Constants.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::DecodeComponents
{
  // Take a composite input and break the luma and chroma back up so that it's "SVideo" (this is the NTSC luma/chroma separation process)
  class CompositeToSVideo
  {
  public:
    CompositeToSVideo(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreatePixelShader(IDR_COMPOSITE_TO_SVIDEO, &compositeToSVideoShader);
    }


    void Apply(GraphicsDevice *device,  const ITexture *compositeIn, ITexture *sVideoOut)
    {
      ConstantData cd = { k_signalSamplesPerColorCycle, 1.0f / float(signalTextureWidth), 1.0f / float(scanlineCount) };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      device->RenderQuadWithPixelShader(
        compositeToSVideoShader,
        sVideoOut,
        {compositeIn},
        {SamplerType::Clamp},
        {constantBuffer});
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match k_signalSamplesPerColorCycle
      float invInputWidth;
      float invInputHeight;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11PixelShader> compositeToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}