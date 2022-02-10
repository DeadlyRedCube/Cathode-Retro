#pragma once

#include <algorithm>

#include "SignalDecode/TVKnobSettings.h"
#include "SignalGeneration/ArtifactSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::SignalDecode
{
  // This takes an SVideo signal and decodes it into a YIQ (NTSC's native color space) output (this is the process of "NTSC color demodulation").
  class SVideoToYIQ
  {
  public:
    SVideoToYIQ(
      GraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      device->CreateConstantBuffer(sizeof(ConstantData), &constantBuffer);
      device->CreateComputeShader(IDR_SVIDEO_TO_YIQ, &sVideoToYIQShader);
    }


    void Apply(GraphicsDevice *device, ProcessContext *buffers, const TVKnobSettings &knobSettings, const SignalGeneration::ArtifactSettings &artifactSettings)
    {
      auto context = device->Context();

      ConstantData data = 
      { 
        SignalGeneration::k_signalSamplesPerColorCycle,
        knobSettings.tint,
        knobSettings.saturation / buffers->saturationScale,
        knobSettings.brightness,
        buffers->blackLevel,
        buffers->whiteLevel,
        buffers->hasDoubledSignal ? artifactSettings.temporalArtifactReduction : 0.0f,
      };
      device->DiscardAndUpdateBuffer(constantBuffer, &data);

      ID3D11ShaderResourceView *srv[] = 
      {
        buffers->hasDoubledSignal ? buffers->fourComponentTex.srv : buffers->twoComponentTex.srv, 
        buffers->hasDoubledSignal ? buffers->scanlinePhasesTwoComponent.srv : buffers->scanlinePhasesOneComponent.srv,
      };

      auto uav = buffers->fourComponentTexScratch.uav.Ptr();
      auto cb = constantBuffer.Ptr();

      context->CSSetShader(sVideoToYIQShader, nullptr, 0);
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetConstantBuffers(0, 1, &cb);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

      context->Dispatch((signalTextureWidth + 7) / 8, (scanlineCount + 7) / 8, 1);

      ZeroType(srv, k_arrayLength<decltype(srv)>);
      uav = nullptr;
      context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
      context->CSSetShaderResources(0, UINT(k_arrayLength<decltype(srv)>), srv);

      std::swap(buffers->fourComponentTex, buffers->fourComponentTexScratch);
    }

  private:
    struct ConstantData
    {
      uint32_t samplesPerColorburstCycle;           // This value should match SignalGeneration::k_signalSamplesPerColorCycle
      float tint;                                   // How much additional tint to apply to the signal (usually a user setting)
      float saturation;                             // The saturation of the output (a user setting)
      float brightness;                             // The brightness adjustment for the output (a user setting)

      float blackLevel;                             // The black "voltage" level of the input signal (this is supplied from the signal generator/reader)
      float whiteLevel;                             // The white "voltage" level of the input signal (this is supplied from the signal generator/reader)

      float temporalArtifactReduction;              // If we have a doubled input (same picture, two phases) blend in the second one (1.0 being a perfect
                                                    //  50/50 blend)
    };

    uint32_t scanlineCount;
    uint32_t signalTextureWidth;
  
    ComPtr<ID3D11ComputeShader> sVideoToYIQShader;
  
    ComPtr<ID3D11Buffer> constantBuffer;
  };
}