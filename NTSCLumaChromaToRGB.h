#pragma once

#include <algorithm>

#include "GraphicsDevice.h"
#include "NTSCRenderBuffers.h"
#include "NTSCSignalGeneration.h"
#include "resource.h"
#include "Util.h"


struct ScreenInfo
{
  float blackLevel;
  float whiteLevel;

  float saturation;
  float brightness;
  float gamma;
  float tint;

  float sharpness;
};

//Takes a Luma/chroma texture located at signalTextureTwoComponentA and outputs an RGB texture at signalTextureColorA
class NTSCLumaChromaToRGB
{
public:
  NTSCLumaChromaToRGB(
    GraphicsDevice *device, 
    uint32_t signalTextureWidthIn, 
    uint32_t scanlineCountIn)
  : scanlineCount(scanlineCountIn)
  , signalTextureWidth(signalTextureWidthIn)
  , blurIQKernelSize(uint32_t(std::round(float(k_signalSamplesPerColorCycle) * 1.5)) | 1)
  {
    device->CreateConstantBuffer(sizeof(LumaChromaToYIQConstants), &lumaChromaToYIQConstantBuffer);
    device->CreateConstantBuffer(sizeof(BlurIQConstants), &blurIQConstantBuffer);
    device->CreateConstantBuffer(sizeof(YIQToRGBConstants), &yiqToRGBConstantBuffer);
    device->CreateConstantBuffer(sizeof(BlurRGBConstants), &blurRGBConstantsBuffer);

    device->CreateComputeShader(IDR_LUMACHROMATOYIQ, &lumaChromaToYIQShader);
    device->CreateComputeShader(IDR_BLURIQ, &blurIQShader);
    device->CreateComputeShader(IDR_YIQTORGB, &yiqToRGBShader);
    device->CreateComputeShader(IDR_BLURRGB, &blurRGBShader);

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


  void Apply(
    GraphicsDevice *device, 
    NTSCRenderBuffers *buffers,
    const ScreenInfo &screenInfo)
  {
    auto context = device->Context();

    // Step 1: luma/chroma to YIQ (source: signalTwoComponentA, target: signalFourComponentA
    {
      LumaChromaToYIQConstants data = { k_signalSamplesPerColorCycle, screenInfo.tint };
      device->DiscardAndUpdateBuffer(lumaChromaToYIQConstantBuffer, &data);

      ID3D11ShaderResourceView *srv[] = {buffers->signalSRVTwoComponentA, buffers->scanlinePhasesSRV};
      auto uav = buffers->signalUAVFourComponentA.Ptr();
      auto cb = lumaChromaToYIQConstantBuffer.Ptr();

      context->CSSetShader(lumaChromaToYIQShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      ZeroType(srv, k_arrayLength<decltype(srv)>);
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);
    }


    // Step 2: Blur IQ (source/target: signalFourComponentA)
#if 0
    {
      BlurIQConstants data = { blurIQKernelSize };
      device->DiscardAndUpdateBuffer(blurIQConstantBuffer, &data);

      ID3D11ShaderResourceView *srv[] = {buffers->signalSRVFourComponentA, blurIQKernelSRV};
      auto uav = buffers->signalUAVFourComponentB.Ptr();
      auto cb = blurIQConstantBuffer.Ptr();

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
      std::swap(buffers->signalTextureFourComponentA, buffers->signalTextureFourComponentB);
      std::swap(buffers->signalUAVFourComponentA, buffers->signalUAVFourComponentB);
      std::swap(buffers->signalSRVFourComponentA, buffers->signalSRVFourComponentB);
    }
#endif

    // Step 3: YIQ to RGB (source: signalFourComponentA, target: signalColorA
    {
      YIQToRGBConstants data = { screenInfo.blackLevel, screenInfo.whiteLevel, screenInfo.saturation, screenInfo.brightness, screenInfo.gamma };
      device->DiscardAndUpdateBuffer(yiqToRGBConstantBuffer, &data);

      auto srv = buffers->signalSRVFourComponentA.Ptr();
      auto uav = buffers->signalUAVColorA.Ptr();
      auto cb = yiqToRGBConstantBuffer.Ptr();

      context->CSSetShader(yiqToRGBShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);


      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);
    }

#if 1
    // Step 4: Blur/Sharpen RGB (source/target: signalUAVColorA)
    if (screenInfo.sharpness != 0)
    {
      BlurRGBConstants data = { -screenInfo.sharpness, k_signalSamplesPerColorCycle };
      device->DiscardAndUpdateBuffer(blurRGBConstantsBuffer, &data);

      auto srv = buffers->signalSRVColorA.Ptr();
      auto uav = buffers->signalUAVColorB.Ptr();
      auto cb = blurRGBConstantsBuffer.Ptr();

      context->CSSetShader(blurRGBShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, 1, &srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      srv = nullptr;
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, 1, &srv);

      std::swap(buffers->signalSRVColorA, buffers->signalSRVColorB);
      std::swap(buffers->signalUAVColorA, buffers->signalUAVColorB);
      std::swap(buffers->signalTextureColorA, buffers->signalTextureColorB);
    }
#endif
  }

private:
  struct LumaChromaToYIQConstants
  {
    uint32_t samplesPerColorburstCycle;
    float tint;
  };

  struct BlurIQConstants
  {
    uint32_t filterWidth;
  };

  struct YIQToRGBConstants
  {
    float blackLevel;
    float whiteLevel;
    float saturation;
    float brightness;
    float gamma;
  };

  struct BlurRGBConstants
  {
    float blurStrength;
    uint32_t texelStepSize;
  };

  uint32_t scanlineCount;
  uint32_t signalTextureWidth;
  uint32_t blurIQKernelSize;
  
  ComPtr<ID3D11ComputeShader> lumaChromaToYIQShader;
  ComPtr<ID3D11ComputeShader> blurIQShader;
  ComPtr<ID3D11ComputeShader> yiqToRGBShader;
  ComPtr<ID3D11ComputeShader> blurRGBShader;
  
  ComPtr<ID3D11Buffer> lumaChromaToYIQConstantBuffer;
  ComPtr<ID3D11Buffer> blurIQConstantBuffer;
  ComPtr<ID3D11Buffer> yiqToRGBConstantBuffer;
  ComPtr<ID3D11Buffer> blurRGBConstantsBuffer;

  ComPtr<ID3D11Texture2D> blurIQKernelTexture;
  ComPtr<ID3D11ShaderResourceView> blurIQKernelSRV;
};
