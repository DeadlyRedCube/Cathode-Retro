#pragma once

#include <algorithm>

#include "SignalGeneration/ArtifactSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"

namespace NTSCify::SignalGeneration
{
  //Takes a Luma/chroma texture located at signalUAVTwoComponentA and replaces it with one that has had the luma/chroma mixed then separated
  class ApplyArtifacts
  {
  public:
    ApplyArtifacts(GraphicsDevice *device, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_APPLY_ARTIFACTS, &applyArtifactsShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers, const ArtifactSettings &options)
    {
      auto context = device->Context();

      // First step: Convert the luma/chroma at signalUAVTwoComponentA into a composite texture at signalUAVOneComponentA
      ConstantData cd = 
      {
        options.ghostSpreadScale,
        options.ghostVisibility,
        noiseSeed,
        options.noiseStrength,
        signalTextureWidth,
        scanlineCount,
      };

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      auto srv = ((buffers->signalType == SignalType::SVideo) ? buffers->twoComponentTex : buffers->oneComponentTex).srv.Ptr();
      auto uav = ((buffers->signalType == SignalType::SVideo) ? buffers->twoComponentTexScratch : buffers->oneComponentTexScratch).uav.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(applyArtifactsShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
      noiseSeed = (noiseSeed + 1) % (60*60);

      if (buffers->signalType == SignalType::SVideo)
      {
        std::swap(buffers->twoComponentTex, buffers->twoComponentTexScratch);
      }
      else
      {
        std::swap(buffers->oneComponentTex, buffers->oneComponentTexScratch);
      }
    }

  private:
    struct ConstantData
    {
      float ghostSpreadScale;
      float ghostBrightness;
      int32_t noiseSeed;
      float noiseScale;
      uint32_t signalTextureWidth;
      uint32_t scanlineCount;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> applyArtifactsShader;
    ComPtr<ID3D11Buffer> constantBuffer;

    int32_t noiseSeed = 0;
  };
}
