#pragma once

#include "NTSCify/Internal/Constants.h"
#include "NTSCify/Internal/SignalLevels.h"
#include "NTSCify/Internal/SignalProperties.h"
#include "NTSCify/TVKnobSettings.h"


namespace NTSCify::Internal::Decoder
{
  class SignalDecoder
  {
  public:
    SignalDecoder(IGraphicsDevice *deviceIn, const SignalProperties &signalPropsIn)
    : device(deviceIn)
    , signalProps(signalPropsIn)
    {
      if (signalProps.type == SignalType::Composite)
      {
        // We need a Composite -> SVideo step (luma/chroma separation), so run that
        compositeToSVideoConstantBuffer = device->CreateConstantBuffer(sizeof(CompositeToSVideoConstantData));
        compositeToSVideoShader = device->CreateShader(ShaderID::CompositeToSVideo);

        decodedSVideoTextureSingle = device->CreateRenderTarget(
          signalProps.scanlineWidth,
          signalProps.scanlineCount,
          1,
          TextureFormat::RG_Float32);
        decodedSVideoTextureDouble = device->CreateRenderTarget(
          signalProps.scanlineWidth,
          signalProps.scanlineCount,
          1,
          TextureFormat::RGBA_Float32);
      }

      // Now initialise the SVideo -> RGB elements
      sVideoToRGBConstantBuffer = device->CreateConstantBuffer(sizeof(SVideoToRGBConstantData));
      sVideoToRGBShader = device->CreateShader(ShaderID::SVideoToRGB);
      rgbTexture = device->CreateRenderTarget(
        signalProps.scanlineWidth,
        signalProps.scanlineCount,
        1,
        TextureFormat::RGBA_Unorm8);
      prevFrameRGBTexture = device->CreateRenderTarget(
        signalProps.scanlineWidth,
        signalProps.scanlineCount,
        1,
        TextureFormat::RGBA_Unorm8);
      scratchRGBTexture = device->CreateRenderTarget(
        signalProps.scanlineWidth,
        signalProps.scanlineCount,
        1,
        TextureFormat::RGBA_Unorm8);

      // Finally, the RGB filtering portions
      filterRGBConstantBuffer = device->CreateConstantBuffer(sizeof(FilterRGBConstantData));
      filterRGBShader = device->CreateShader(ShaderID::FilterRGB);
    }

    void SetKnobSettings(const TVKnobSettings &settings)
      { knobSettings = settings; }

    const ITexture *CurrentFrameRGBOutput() const
      { return rgbTexture.get(); }

    const ITexture *PreviousFrameRGBOutput() const
      { return prevFrameRGBTexture.get(); }

    void Decode(const ITexture *inputSignal, const ITexture *inputPhases, const SignalLevels &levels)
    {
      std::swap(rgbTexture, prevFrameRGBTexture);

      const ITexture *sVideoTexture;
      if (signalProps.type == SignalType::Composite)
      {
        ITexture *outTex = (levels.temporalArtifactReduction > 0.0f)
          ? decodedSVideoTextureDouble.get()
          : decodedSVideoTextureSingle.get();
        sVideoTexture = outTex;
        CompositeToSVideo(inputSignal, levels.temporalArtifactReduction > 0.0f);
      }
      else
      {
        sVideoTexture = inputSignal;
      }

      SVideoToRGB(sVideoTexture, inputPhases, levels);

      if (knobSettings.sharpness != 0.0f)
      {
        FilterRGB();
      }
    }

  private:
    void CompositeToSVideo(const ITexture *inputSignal, bool isDoubled)
    {
      compositeToSVideoConstantBuffer->Update(CompositeToSVideoConstantData{ k_signalSamplesPerColorCycle });
      device->RenderQuad(
        compositeToSVideoShader.get(),
        (isDoubled ? decodedSVideoTextureDouble : decodedSVideoTextureSingle).get(),
        {{inputSignal, SamplerType::LinearClamp}},
        compositeToSVideoConstantBuffer.get());
    }


    void SVideoToRGB(const ITexture *sVideoTexture, const ITexture *inputPhases, const SignalLevels &levels)
    {
      sVideoToRGBConstantBuffer->Update(
        SVideoToRGBConstantData {
          .samplesPerColorburstCycle = k_signalSamplesPerColorCycle,
          .tint = knobSettings.tint,

          // Saturation needs brightness scaled into it as well or else the output is weird when the brightness is set below 1.0
          .saturation = knobSettings.saturation / levels.saturationScale * knobSettings.brightness,
          .brightness = knobSettings.brightness,
          .blackLevel = levels.blackLevel,
          .whiteLevel = levels.whiteLevel,
          .temporalArtifactReduction = levels.temporalArtifactReduction,
        });

      device->RenderQuad(
        sVideoToRGBShader.get(),
        rgbTexture.get(),
        {
          {sVideoTexture, SamplerType::LinearClamp},
          {inputPhases, SamplerType::LinearClamp},
        },
        sVideoToRGBConstantBuffer.get());
    }


    void FilterRGB()
    {
      filterRGBConstantBuffer->Update(
        FilterRGBConstantData {
          .blurStrength = -knobSettings.sharpness,
          .blurSampleStepSize = signalProps.colorCyclesPerInputPixel * float(k_signalSamplesPerColorCycle)
        });

      device->RenderQuad(
        filterRGBShader.get(),
        scratchRGBTexture.get(),
        {{rgbTexture.get(), SamplerType::LinearClamp}},
        filterRGBConstantBuffer.get());

      // Our output is the new RGB Texture.
      std::swap(rgbTexture, scratchRGBTexture);
    }

    IGraphicsDevice *device;

    std::unique_ptr<ITexture> rgbTexture;
    std::unique_ptr<ITexture> prevFrameRGBTexture;
    std::unique_ptr<ITexture> scratchRGBTexture;
    SignalProperties signalProps;
    TVKnobSettings knobSettings;

    // Step 1: Composite to SVideo elements
    struct CompositeToSVideoConstantData
    {
      uint32_t outputTexelsPerColorburstCycle;        // This value should match k_signalSamplesPerColorCycle
    };

    std::unique_ptr<IShader> compositeToSVideoShader;
    std::unique_ptr<IConstantBuffer> compositeToSVideoConstantBuffer;
    std::unique_ptr<ITexture> decodedSVideoTextureSingle;
    std::unique_ptr<ITexture> decodedSVideoTextureDouble;

    // Step 2: SVideo to RGB Elements
    struct SVideoToRGBConstantData
    {
      uint32_t samplesPerColorburstCycle;           // This value should match k_signalSamplesPerColorCycle
      float tint;                                   // How much additional tint to apply to the signal (usually a user setting)
      float saturation;                             // The saturation of the output (a user setting)
      float brightness;                             // The brightness adjustment for the output (a user setting)

      float blackLevel;                             // The black "voltage" level of the input signal (from the signal generator/reader)
      float whiteLevel;                             // The white "voltage" level of the input signal (from the signal generator/reader)

      float temporalArtifactReduction;              // If we have a doubled input (same picture, two phases) blend in the second one (1.0
                                                    //  being a perfect 50/50 blend)
    };

    std::unique_ptr<IShader> sVideoToRGBShader;
    std::unique_ptr<IConstantBuffer> sVideoToRGBConstantBuffer;

    // Step 3: Filter RGB Elements
    struct FilterRGBConstantData
    {
      float blurStrength;
      float blurSampleStepSize;
    };

    std::unique_ptr<IShader> filterRGBShader;
    std::unique_ptr<IConstantBuffer> filterRGBConstantBuffer;
  };
}