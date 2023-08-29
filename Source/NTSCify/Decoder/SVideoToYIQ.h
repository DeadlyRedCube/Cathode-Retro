#pragma once

#include <algorithm>

#include "NTSCify/ArtifactSettings.h"
#include "NTSCify/SignalLevels.h"
#include "NTSCify/TVKnobSettings.h"
#include "GraphicsDevice.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify::Decoder
{
  // This takes an SVideo signal and decodes it into a YIQ (NTSC's native color space) output (this is the process of "NTSC color demodulation").
  class SVideoToYIQ
  {
  public:
    SVideoToYIQ(
      IGraphicsDevice *device, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : scanlineCount(scanlineCountIn)
    , signalTextureWidth(signalTextureWidthIn)
    {
      constantBuffer = device->CreateConstantBuffer(sizeof(ConstantData));
      sVideoToYIQShader = device->CreateShader(ShaderID::SVideoToYIQ);
    }


    void Apply(
      IGraphicsDevice *device, 
      const SignalLevels &levels,
      const ITexture *signalInput,
      const ITexture *phasesInput,
      ITexture *yiqOutput,
      const TVKnobSettings &knobSettings)
    {
      ConstantData data = 
      { 
        k_signalSamplesPerColorCycle,
        knobSettings.tint,
        // Saturation needs brightness scaled into it as well or else the output is weird when the brightness is set below 1.0
        knobSettings.saturation / levels.saturationScale * knobSettings.brightness,
        knobSettings.brightness,
        levels.blackLevel,
        levels.whiteLevel,
        levels.temporalArtifactReduction,
      };

      device->DiscardAndUpdateBuffer(constantBuffer.get(), &data);

      device->RenderQuad(
        sVideoToYIQShader.get(),
        yiqOutput,
        {signalInput, phasesInput},
        {SamplerType::LinearClamp},
        {constantBuffer.get()});
    }

  private:
    struct ConstantData
    {
      uint32_t samplesPerColorburstCycle;           // This value should match k_signalSamplesPerColorCycle
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
  
    std::unique_ptr<IShader> sVideoToYIQShader;
  
    std::unique_ptr<IConstantBuffer> constantBuffer;
  };
}