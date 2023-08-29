#pragma once

#include "NTSCify/SignalProperties.h"
#include "NTSCify/Decoder/CompositeToSVideo.h"
#include "NTSCify/Decoder/FilterRGB.h"
#include "NTSCify/Decoder/SVideoToYIQ.h"
#include "NTSCify/Decoder/YIQToRGB.h"


namespace NTSCify
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
        compositeToSVideo = std::make_unique<DecodeComponents::CompositeToSVideo>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
        decodedSVideoTextureSingle = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RG_Float32, TextureFlags::RenderTarget);
        decodedSVideoTextureDouble = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RGBA_Float32, TextureFlags::RenderTarget);
      }

      sVideoToYIQ = std::make_unique<DecodeComponents::SVideoToYIQ>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
      yiqToRGB = std::make_unique<DecodeComponents::YIQToRGB>(device, signalProps.scanlineWidth, signalProps.scanlineCount);
      filterRGB = std::make_unique<DecodeComponents::FilterRGB>(device, signalProps.colorCyclesPerInputPixel, signalProps.scanlineWidth, signalProps.scanlineCount);

      yiqTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RGBA_Float32, TextureFlags::RenderTarget);
      rgbTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);
      prevFrameRGBTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);
      scratchRGBTexture = device->CreateTexture(signalProps.scanlineWidth, signalProps.scanlineCount, 1, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);
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
        ITexture *outTex = (levels.isDoubled) ? decodedSVideoTextureDouble.get() : decodedSVideoTextureSingle.get();
        sVideoTexture = outTex;
        compositeToSVideo->Apply(device, inputSignal, outTex);
      }
      else
      {
        sVideoTexture = inputSignal;
      }

      sVideoToYIQ->Apply(device, levels, sVideoTexture, inputPhases, yiqTexture.get(), knobSettings);
      yiqToRGB->Apply(device, yiqTexture.get(), rgbTexture.get());
      if (filterRGB->Apply(device, rgbTexture.get(), scratchRGBTexture.get(), knobSettings))
      {
        // We applied RGB to scratch so swap scratch in for our RGB texture it's now our output
        std::swap(rgbTexture, scratchRGBTexture);
      }
    }

  private:
    IGraphicsDevice *device;
    std::unique_ptr<ITexture> decodedSVideoTextureSingle;
    std::unique_ptr<ITexture> decodedSVideoTextureDouble;
    std::unique_ptr<ITexture> yiqTexture;
    std::unique_ptr<ITexture> rgbTexture;
    std::unique_ptr<ITexture> prevFrameRGBTexture;
    std::unique_ptr<ITexture> scratchRGBTexture;

    std::unique_ptr<DecodeComponents::CompositeToSVideo> compositeToSVideo;
    std::unique_ptr<DecodeComponents::SVideoToYIQ> sVideoToYIQ;
    std::unique_ptr<DecodeComponents::YIQToRGB> yiqToRGB;
    std::unique_ptr<DecodeComponents::FilterRGB> filterRGB;

    SignalProperties signalProps;
    TVKnobSettings knobSettings;
  };
}