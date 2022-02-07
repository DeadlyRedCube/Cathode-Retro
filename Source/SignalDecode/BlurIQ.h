#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "SignalGeneration/RGBToSVideo.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  class BlurIQ
  {
  public:
    BlurIQ(
      GraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    , blurIQKernelSize(uint32_t(std::round(float(SignalGeneration::k_signalSamplesPerColorCycle) * 1.5)) | 1)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_BLUR_IQ, &blurIQShader);

      auto kernel = CalculateGaussianKernel(int32_t(blurIQKernelSize), 8.0f);
      device->CreateTexture2D(
        blurIQKernelSize,
        1, 
        DXGI_FORMAT_R32_FLOAT, 
        GraphicsDevice::TextureFlags::None, 
        &blurIQKernelTexture, 
        &blurIQKernelSRV,
        nullptr,
        kernel.data(),
        uint32_t(kernel.size()));
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers)
    {
      auto context = device->Context();

      ConstantData data = { blurIQKernelSize };
      device->DiscardAndUpdateBuffer(constantBuffer, &data);

      ID3D11ShaderResourceView *srv[] = {buffers->fourComponentTex.srv, blurIQKernelSRV};
      auto uav = buffers->fourComponentTexScratch.uav.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(blurIQShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);


      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      ZeroType(srv, k_arrayLength<decltype(srv)>);
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

      // Swap B -> A so that the output texture is on A
      std::swap(buffers->fourComponentTex, buffers->fourComponentTexScratch);
    }


  private:
    struct ConstantData
    {
      uint32_t filterWidth;
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    uint32_t blurIQKernelSize;
  
    ComPtr<ID3D11ComputeShader> blurIQShader;
    ComPtr<ID3D11Buffer> constantBuffer;

    ComPtr<ID3D11Texture2D> blurIQKernelTexture;
    ComPtr<ID3D11ShaderResourceView> blurIQKernelSRV;
  };
}