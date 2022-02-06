#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "NTSCRenderBuffers.h"
#include "NTSCSignalGeneration.h"
#include "resource.h"
#include "Util.h"


//Takes an input rgb texture and converts it into a Luma/chroma texture located at signalUAVTwoComponentA.
class NTSCRGBSignalGenerator
{
public:
  NTSCRGBSignalGenerator(GraphicsDevice *device, uint32_t rgbTextureWidth, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
  : scanlineCount(scanlineCountIn)
  , signalTextureWidth(signalTextureWidthIn)
  {
    device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
    device->CreateComputeShader(IDR_RGBTOLUMACHROMA, &rgbToLumaChromaShader);

    ConstantData cd = { k_signalSamplesPerColorCycle, rgbTextureWidth, signalTextureWidth };
    device->DiscardAndUpdateBuffer(constantBuffer, &cd);
  }


  void Generate(GraphicsDevice *device, ID3D11ShaderResourceView *rgbSRV, NTSCRenderBuffers *buffers, float initialFramePhase, float phaseIncrementPerScanline)
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
    auto uav = buffers->signalUAVTwoComponentA.Ptr();
    auto cb = constantBuffer.Ptr();

    context->CSSetShader(rgbToLumaChromaShader, nullptr, 0);
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    context->CSSetConstantBuffers(0, 1, &cb);
    context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);


    context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

    ZeroType(srv, k_arrayLength<decltype(srv)>);
    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);
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
  ComPtr<ID3D11ComputeShader> rgbToLumaChromaShader;
  ComPtr<ID3D11Buffer> constantBuffer;
};
