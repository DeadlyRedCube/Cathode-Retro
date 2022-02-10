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
  // Take an RGB input texture (usually the output of the game or emulator) and convert it into either an SVideo (separate luma/chroma) or Composite
  //  (a single combined channel) output. We will also, if temporalArtifactReduction is non-zero, generate a second signal into the output texture:
  //  this represents the same /frame/ of data, but with a different starting phase, so that we can mix them together to reduce the flickering that
  //  the output of NES-style timings will give you normally.
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


    void Generate(GraphicsDevice *device, ID3D11ShaderResourceView *rgbSRV, ProcessContext *buffers, float initialFramePhase, float phaseIncrementPerScanline, const ArtifactSettings &artifactSettings)
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

      // Now run the actual shader
      ID3D11ShaderResourceView *srv[] = {rgbSRV, scanlinePhaseSRV};
      auto uav = targetUAV;
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

      prevFrameStartPhase = initialFramePhase;
    }

  private:
    struct ConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match SignalGeneration::k_signalSamplesPerColorCycle
      uint32_t inputWidth;                            // The width of the input, in texels
      uint32_t outputWidth;                           // The width of the output, in texels
      float compositeBlend;                           // 0 if we're outputting to SVideo, 1 if it's composite
    };

    uint32_t rgbTextureWidth;
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> rgbToSVideoShader;
    ComPtr<ID3D11Buffer> constantBuffer;
    float prevFrameStartPhase = 0.0f;
  };
}