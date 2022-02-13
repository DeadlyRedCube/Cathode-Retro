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
      device->CreateConstantBuffer(std::max(sizeof(RGBToSVideoConstantData), sizeof(GeneratePhaseTextureConstantData)), &constantBuffer);
      device->CreateComputeShader(IDR_RGB_TO_SVIDEO_OR_COMPOSITE, &rgbToSVideoShader);
      device->CreateComputeShader(IDR_GENERATE_PHASE_TEXTURE, &generatePhaseTextureShader);
    }


    void Generate(
      GraphicsDevice *device, 
      ID3D11ShaderResourceView *rgbSRV, 
      ProcessContext *buffers, 
      float initialFramePhase, 
      float phaseIncrementPerScanline, 
      const ArtifactSettings &artifactSettings)
    {
      auto context = device->Context();

      ID3D11ShaderResourceView *scanlinePhaseSRV;
      ID3D11UnorderedAccessView *scanlinePhaseUAV;
      ID3D11UnorderedAccessView *targetUAV;
      if (artifactSettings.temporalArtifactReduction > 0.0f && prevFrameStartPhase != initialFramePhase)
      {
        buffers->hasDoubledSignal = true;
        scanlinePhaseSRV = buffers->scanlinePhasesTwoComponent.srv;
        scanlinePhaseUAV = buffers->scanlinePhasesTwoComponent.uav;
        targetUAV = (buffers->signalType == SignalType::Composite) ? buffers->twoComponentTex.uav.Ptr() : buffers->fourComponentTex.uav.Ptr();
      }
      else
      {
        buffers->hasDoubledSignal = false;
        scanlinePhaseSRV = buffers->scanlinePhasesOneComponent.srv;
        scanlinePhaseUAV = buffers->scanlinePhasesOneComponent.uav;
        targetUAV = (buffers->signalType == SignalType::Composite) ? buffers->oneComponentTex.uav.Ptr() : buffers->twoComponentTex.uav.Ptr();
      }

      // Update our scanline phases texture
      {
        GeneratePhaseTextureConstantData cd = 
        { 
          initialFramePhase,
          prevFrameStartPhase,
          phaseIncrementPerScanline,
          k_signalSamplesPerColorCycle,
          artifactSettings.instabilityScale,
          noiseSeed,
          signalTextureWidth,
          scanlineCount,
        };

        device->DiscardAndUpdateBuffer(constantBuffer, &cd);

        auto uav = scanlinePhaseUAV;
        auto cb = constantBuffer.Ptr();
        context->CSSetShader(generatePhaseTextureShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);

        context->Dispatch((scanlineCount + 7) / 8, 1, 1);

        uav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      }

      // Now run the actual shader
      {
        RGBToSVideoConstantData cd = 
        { 
          k_signalSamplesPerColorCycle, 
          rgbTextureWidth, 
          signalTextureWidth,  
          scanlineCount,
          (buffers->signalType == SignalType::Composite) ? 1.0f : 0.0f,
          artifactSettings.instabilityScale,
          noiseSeed,
        };

        device->DiscardAndUpdateBuffer(constantBuffer, &cd);

        ID3D11ShaderResourceView *srv[] = {rgbSRV, scanlinePhaseSRV};
        auto uav = targetUAV;
        auto cb = constantBuffer.Ptr();

        {
          ID3D11SamplerState *st[] = {buffers->samplerStateClamp};
          context->CSSetSamplers(0, UINT(k_arrayLength<decltype(st)>), st);
        }

        context->CSSetShader(rgbToSVideoShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);
        context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

        context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

        ZeroType(srv, k_arrayLength<decltype(srv)>);
        uav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);
      }

      buffers->blackLevel = 0.0f;
      buffers->whiteLevel = 1.0f;
      buffers->saturationScale = 0.5f;

      prevFrameStartPhase = initialFramePhase;

      noiseSeed = (noiseSeed + 1) & 0x000FFFFF;
    }

  private:
    struct RGBToSVideoConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match SignalGeneration::k_signalSamplesPerColorCycle
      uint32_t inputWidth;                            // The width of the input, in texels
      uint32_t outputWidth;                           // The width of the output, in texels
      uint32_t scanlineCount;                         // How many scanlines
      float compositeBlend;                           // 0 if we're outputting to SVideo, 1 if it's composite
      float instabilityScale;
      uint32_t noiseSeed;
    };

    struct GeneratePhaseTextureConstantData
    {
      float g_initialFrameStartPhase;                 // The phase at the start of the first scanline of this frame
      float g_prevFrameStartPhase;                    // The phase at the start of the previous scanline of this frame (if relevant)
      float g_phaseIncrementPerScanline;              // The amount to increment the phase each scanline
      uint32_t g_samplesPerColorburstCycle;           // Should match k_signalSamplesPerColorCycle
      float instabilityScale;
      uint32_t noiseSeed;
      uint32_t signalTextureWidth;
      uint32_t scanlineCount;                         // How many scanlines
    };

    uint32_t rgbTextureWidth;
    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
    ComPtr<ID3D11ComputeShader> rgbToSVideoShader;
    ComPtr<ID3D11ComputeShader> generatePhaseTextureShader;
    ComPtr<ID3D11Buffer> constantBuffer;
    float prevFrameStartPhase = 0.0f;

    uint32_t noiseSeed = 0;
  };
}