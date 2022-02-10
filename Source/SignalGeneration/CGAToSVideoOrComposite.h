#pragma once

#include <algorithm>

#include "SignalGeneration/ArtifactSettings.h"
#include "SignalGeneration/Constants.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalGeneration
{
  enum class CGAInputType
  {
    RGB32Bit,
    RGBI4Bit
  };


  enum class CGAEmulationType
  {
    OldStyle, // Intensity = 0.28 * I, Chroma = *.72 * waveform
    NewStyle, // Intensity = 0.32*I + 0.1*R + 0.22*G + 0.07*B, chroma
  };


  // Converts a CGA signal to SVideo or Composite
  // $TODO The shader itself doesn't currently work right - I tried to cheat it and it is still wrong and bad.
  template <CGAInputType inputType>
  class CGAToSVideoOrComposite
  {
  public:
    CGAToSVideoOrComposite(GraphicsDevice *device, uint32_t inputWidthInPixelsIn, uint32_t signalTextureWidthIn, uint32_t scanlineCountIn)
    : inputWidthInPixels(inputWidthInPixelsIn)
    , scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_CGA_TO_SVIDEO_OR_COMPOSITE, &cgaToSVideoShader);

      if constexpr (inputType == CGAInputType::RGB32Bit)
      {
        device->CreateTexture2D(
          inputWidthInPixels / 2,
          scanlineCount,
          DXGI_FORMAT_R8_UINT,
          GraphicsDevice::TextureFlags::UAV,
          &optionals.cgaTexture,
          &optionals.cgaSrv,
          &optionals.cgaUav);

        device->CreateComputeShader(IDR_RGB_TO_CGA, &optionals.rgbToCGAShader);
      }
    }


    void Generate(
      GraphicsDevice *device, 
      ID3D11ShaderResourceView *inputSRV, 
      CGAEmulationType emulationType,
      ProcessContext *buffers, 
      float initialFramePhase, 
      float phaseIncrementPerScanline, 
      const ArtifactSettings &artifactSettings)
    {
      auto context = device->Context();

      ConstantData cd = 
      { 
        k_signalSamplesPerColorCycle, 
        inputWidthInPixels, 
        signalTextureWidth,  
        (emulationType == CGAEmulationType::OldStyle) ? 0U : 1U,
        (buffers->signalType == SignalType::Composite) ? 1.0f : 0.0f,
      };

      device->DiscardAndUpdateBuffer(constantBuffer, &cd);

      // Update our scanline phases texture

      // $TODO This can be done as a shader as well instead of using a dynamic texture
      ID3D11ShaderResourceView *scanlinePhaseSRV;
      ID3D11UnorderedAccessView *targetUAV;
      if (artifactSettings.temporalArtifactReduction > 0.0f && prevFrameStartPhase != initialFramePhase)
      {
        buffers->hasDoubledSignal = true;
        scanlinePhaseSRV = buffers->scanlinePhasesTwoComponent.srv;
        targetUAV = (buffers->signalType == SignalType::Composite) ? buffers->twoComponentTex.uav.Ptr() : buffers->fourComponentTex.uav.Ptr();

        D3D11_MAPPED_SUBRESOURCE map;
        context->Map(buffers->scanlinePhasesTwoComponent.texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        float *phaseTex = static_cast<float *>(map.pData);

        float phaseA = initialFramePhase;
        float phaseB = prevFrameStartPhase;
        for (uint32_t i = 0; i < scanlineCount; i++)
        {
          phaseTex[i * 2 + 0] = phaseA;
          phaseTex[i * 2 + 1] = phaseB;
          phaseA += phaseIncrementPerScanline;
          phaseB += phaseIncrementPerScanline;
        }

        context->Unmap(buffers->scanlinePhasesTwoComponent.texture, 0);
      }
      else
      {
        buffers->hasDoubledSignal = false;
        scanlinePhaseSRV = buffers->scanlinePhasesOneComponent.srv;
        targetUAV = (buffers->signalType == SignalType::Composite) ? buffers->oneComponentTex.uav.Ptr() : buffers->twoComponentTex.uav.Ptr();

        D3D11_MAPPED_SUBRESOURCE map;
        context->Map(buffers->scanlinePhasesOneComponent.texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        float *phaseTex = static_cast<float *>(map.pData);

        float phase = initialFramePhase;
        for (uint32_t i = 0; i < scanlineCount; i++)
        {
          phaseTex[i] = phase;
          phase += phaseIncrementPerScanline;
        }

        context->Unmap(buffers->scanlinePhasesOneComponent.texture, 0);
      }

      ID3D11ShaderResourceView *srv[2] = {inputSRV, scanlinePhaseSRV};
      if constexpr (inputType == CGAInputType::RGB32Bit)
      {
        auto uav = optionals.cgaUav.Ptr();

        context->CSSetShader(optionals.rgbToCGAShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);


        context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

        ZeroType(srv, k_arrayLength<decltype(srv)>);
        uav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv); 

        srv[0] = optionals.cgaSrv;
      }

      auto uav = targetUAV;
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(cgaToSVideoShader, nullptr, 0);
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

      prevFrameStartPhase = initialFramePhase;
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;
      uint32_t inputWidth;
      uint32_t outputWidth;
      uint32_t useNewCGAEmulation;
      float compositeBlend;
    };

    template <CGAInputType>
    struct Optionals
      { };

    template <>
    struct Optionals<CGAInputType::RGB32Bit>
    {
      ComPtr<ID3D11ComputeShader> rgbToCGAShader;
      ComPtr<ID3D11Texture2D> cgaTexture;
      ComPtr<ID3D11ShaderResourceView> cgaSrv;
      ComPtr<ID3D11UnorderedAccessView> cgaUav;
    };

    uint32_t inputWidthInPixels;
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> cgaToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
    float prevFrameStartPhase = 0.0f;
    Optionals<inputType> optionals;
  };
}