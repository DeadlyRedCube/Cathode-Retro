#pragma once

#include <algorithm>

#include "SignalGeneration/Constants.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalGeneration
{
  class RGBToSVideoOrComposite
  {
  public:
    RGBToSVideoOrComposite(GraphicsDevice *device, uint32_t rgbTextureWidthIn, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : rgbTextureWidth(rgbTextureWidthIn)
    , scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_RGB_TO_SVIDEO_OR_COMPOSITE, &rgbToSVideoShader);
    }


    void Generate(GraphicsDevice *device, ID3D11ShaderResourceView *rgbSRV, ProcessContext *buffers, float initialFramePhase, float phaseIncrementPerScanline)
    {
      auto context = device->Context();

      ConstantData cd = 
      { 
        k_signalSamplesPerColorCycle, 
        rgbTextureWidth, 
        signalTextureWidth,  
        (buffers->signalType == SignalType::Composite) ? 1.0f : 0.0f,
      };

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

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
      auto uav = (buffers->signalType == SignalType::Composite) ? buffers->oneComponentTex.uav.Ptr() : buffers->twoComponentTex.uav.Ptr();
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
      float compositeBlend;
    };

    uint32_t rgbTextureWidth;
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> rgbToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}