#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


//Takes an input rgb texture and converts it into a Luma/chroma texture located at signalUAVTwoComponentA.
namespace NTSCify::SignalGeneration
{
  static constexpr uint32_t k_signalSamplesPerColorCycle = 8;

  class RGBToSVideo
  {
  public:
    RGBToSVideo(GraphicsDevice *device, uint32_t rgbTextureWidth, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_RGB_TO_SVIDEO, &rgbToSVideoShader);

      ConstantData cd = { k_signalSamplesPerColorCycle, rgbTextureWidth, signalTextureWidth };
      device->DiscardAndUpdateBuffer(constantBuffer, &cd);
    }


    void Generate(GraphicsDevice *device, ID3D11ShaderResourceView *rgbSRV, ProcessContext *buffers, float initialFramePhase, float phaseIncrementPerScanline)
    {
      auto context = device->Context();

      // Update our scanline phases texture
      {
        D3D11_MAPPED_SUBRESOURCE map;
        context->Map(buffers->scanlinePhasesTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        float *phaseTex = static_cast<float *>(map.pData);

        float phase = initialFramePhase;
        for (uint32_t i = 0; i < scanlineCount; i++)
        {
          phaseTex[i] = phase;
          phase += phaseIncrementPerScanline;
        }

        context->Unmap(buffers->scanlinePhasesTexture, 0);
      }

      ID3D11ShaderResourceView *srv[] = {rgbSRV, buffers->scanlinePhasesSRV};
      auto uav = buffers->twoComponentTexA.uav.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(rgbToSVideoShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);


      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      ZeroType(srv, k_arrayLength<decltype(srv)>);
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

      buffers->blackLevel = 0.0f;
      buffers->whiteLevel = 1.0f;
      buffers->saturationScale = 0.5f;
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;
      uint32_t inputWidth;
      uint32_t outputWidth;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> rgbToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}