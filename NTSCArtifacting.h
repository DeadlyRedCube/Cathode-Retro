#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "NTSCRenderBuffers.h"
#include "NTSCSignalGeneration.h"
#include "resource.h"
#include "Util.h"

struct ArtifactingOptions
{
  float ghostVisibility = 0.0f;
  float ghostSpreadScale = 0.650f;
  float noiseScale = 0.0f;
};

//Takes a Luma/chroma texture located at signalUAVTwoComponentA and replaces it with one that has had the luma/chroma mixed then separated
class NTSCArtifacting
{
public:
  NTSCArtifacting(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
  : scanlineCount(scanlineCountIn)
  , signalTextureWidth(signalTextureWidthIn)
  {
    device->CreateConstantBuffer(std::max(sizeof(LumaChromaToCompositeConstantData), sizeof(SeparateLumaChromaConstantData)), &constantBuffer);
    device->CreateComputeShader(IDR_LUMACHROMATOCOMPOSITE, &lumaChromaToCompositeShader);
    device->CreateComputeShader(IDR_SEPARATELUMACHROMA, &separateLumaChromaShader);
  }


  void Apply(GraphicsDevice *device, NTSCRenderBuffers *buffers, const ArtifactingOptions &options)
  {
    auto context = device->Context();

    // First step: Convert the luma/chroma at signalUAVTwoComponentA into a composite texture at signalUAVOneComponentA
    {
      LumaChromaToCompositeConstantData cd = 
      {
        options.ghostSpreadScale,
        options.ghostVisibility,
        noiseSeed,
        options.noiseScale,
        signalTextureWidth,
        scanlineCount,
      };

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      auto srv = buffers->signalSRVTwoComponentA.Ptr();
      auto uav = buffers->signalUAVOneComponentA.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(lumaChromaToCompositeShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
    }

    // $TODO Any signal-crappening (snow/tracking issues/ghosting/etc) should happen here

    // Last step: Separate the luma/chroma back out from the composite signal, back to signalUAVTwoComponentA
    {
      SeparateLumaChromaConstantData cd = { k_signalSamplesPerColorCycle };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);


      auto srv = buffers->signalSRVOneComponentA.Ptr();
      auto uav = buffers->signalUAVTwoComponentA.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(separateLumaChromaShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);


      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
    }

    noiseSeed = (noiseSeed + 1) % (60*60);
  }

private:
  struct LumaChromaToCompositeConstantData
  {
    float ghostSpreadScale;
    float ghostBrightness;
    int32_t noiseSeed;
    float noiseScale;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
  };

  struct SeparateLumaChromaConstantData
  {
    uint32_t outputTexelsPerColorburstCycle;
  };

  uint32_t scanlineCount;
  uint32_t signalTextureWidth;
  ComPtr<ID3D11ComputeShader> lumaChromaToCompositeShader;
  ComPtr<ID3D11ComputeShader> separateLumaChromaShader;
  ComPtr<ID3D11Buffer> constantBuffer;

  int32_t noiseSeed = 0;
};
