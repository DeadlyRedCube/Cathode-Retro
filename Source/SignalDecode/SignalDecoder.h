#pragma once

#include "SignalGeneration/SignalProperties.h"
#include "SignalDecode/CompositeToSVideo.h"
#include "SignalDecode/FilterRGB.h"
#include "SignalDecode/SVideoToYIQ.h"
#include "SignalDecode/YIQToRGB.h"


namespace NTSCify::SignalDecode
{
  class SignalDecoder
  {
  public:
    SignalDecoder(GraphicsDevice *deviceIn, const SignalGeneration::SignalProperties &signalPropsIn)
    : device(deviceIn)
    , signalProps(signalPropsIn)
    {
      if (signalProps.type == SignalGeneration::SignalType::Composite)
      {
        compositeToSVideo = std::make_unique<CompositeToSVideo>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
        decodedSVideoTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
        decodedSVideoTextureDouble = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);
      }

      sVideoToYIQ = std::make_unique<SVideoToYIQ>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
      yiqToRGB = std::make_unique<YIQToRGB>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
      filterRGB = std::make_unique<FilterRGB>(device, signalProps.colorCyclesPerInputPixel, signalProps.scanlineWidth, signalProps.scanlineCount);

      yiqTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);
      rgbTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
      prevFrameRGBTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
      scratchRGBTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
    }

    void SetKnobSettings(const TVKnobSettings &settings)
      { knobSettings = settings; }

    const ITexture *CurrentFrameRGBOutput() const
      { return rgbTexture.get(); }

    const ITexture *PreviousFrameRGBOutput() const
      { return prevFrameRGBTexture.get(); }

    void Decode(const ITexture *inputSignal, const ITexture *inputPhases, const SignalGeneration::SignalLevels &levels)
    {
      std::swap(rgbTexture, prevFrameRGBTexture);
      const ITexture *sVideoTexture;
      if (signalProps.type == SignalGeneration::SignalType::Composite)
      {
        ITexture *outTex = (levels.isDoubled) ? decodedSVideoTextureDouble.get() : decodedSVideoTextureSingle.get();
        sVideoTexture = outTex;
        compositeToSVideo->Apply(device, inputSignal, outTex);
      }
      else
      {
        sVideoTexture = inputSignal;
      }

      sVideoToYIQ->Apply(device, levels, sVideoTexture, inputPhases, yiqTexture.get(), knobSettings);
      yiqToRGB->Apply(device, yiqTexture.get(), rgbTexture.get(), knobSettings);
      if (filterRGB->Apply(device, rgbTexture.get(), scratchRGBTexture.get(), knobSettings))
      {
        // We applied RGB to scratch so swap scratch in for our RGB texture it's now our output
        std::swap(rgbTexture, scratchRGBTexture);
      }
    }

  private:
    GraphicsDevice *device;
    std::unique_ptr<ITexture> decodedSVideoTextureSingle;
    std::unique_ptr<ITexture> decodedSVideoTextureDouble;
    std::unique_ptr<ITexture> yiqTexture;
    std::unique_ptr<ITexture> rgbTexture;
    std::unique_ptr<ITexture> prevFrameRGBTexture;
    std::unique_ptr<ITexture> scratchRGBTexture;

    std::unique_ptr<CompositeToSVideo> compositeToSVideo;
    std::unique_ptr<SVideoToYIQ> sVideoToYIQ;
    std::unique_ptr<YIQToRGB> yiqToRGB;
    std::unique_ptr<FilterRGB> filterRGB;

    SignalGeneration::SignalProperties signalProps;
    TVKnobSettings knobSettings;
  };
}