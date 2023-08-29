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
    CompositeToSVideo(IGraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      constantBuffer = device->CreateConstantBuffer(sizeof(ConstantData));
      compositeToSVideoShader = device->CreateShader(ShaderID::CompositeToSVideo);
    }


    void Apply(IGraphicsDevice *device,  const ITexture *compositeIn, ITexture *sVideoOut)
    {
      ConstantData cd = { k_signalSamplesPerColorCycle, 1.0f / float(signalTextureWidth), 1.0f / float(scanlineCount) };
      device->DiscardAndUpdateBuffer(constantBuffer.get(), &cd);

      device->RenderQuad(
        compositeToSVideoShader.get(),
        sVideoOut,
        {compositeIn},
        {SamplerType::LinearClamp},
        {constantBuffer.get()});
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
    std::unique_ptr<IShader> compositeToSVideoShader;
    std::unique_ptr<IConstantBuffer> constantBuffer;
  };
}