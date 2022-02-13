#pragma once

#include "GraphicsDevice.h"
#include "SignalGeneration/SourceSettings.h"

namespace NTSCify
{
  class ProcessContext
  {
  public:
    ProcessContext(
      GraphicsDevice *device, 
      SignalGeneration::SignalType sigalType, 
      uint32_t inputTextureWidth, 
      uint32_t scanlineCount, 
      uint32_t colorCyclesPerInputPixel, 
      uint32_t phaseGenerationDenominator);

    // $TODO stop cheating and do real access control for this thing
  // private:
    SignalGeneration::SignalType signalType;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
    bool hasDoubledSignal = false;

    // Set by signal generation
    float whiteLevel;
    float blackLevel;
    float saturationScale;

    // All these shaders use the same vertex buffer and input layout.
    std::unique_ptr<ITexture> scanlinePhasesOneComponent;
    std::unique_ptr<ITexture> scanlinePhasesTwoComponent;

    std::unique_ptr<ITexture> oneComponentTex;
    std::unique_ptr<ITexture> oneComponentTexScratch;

    std::unique_ptr<ITexture> twoComponentTex;
    std::unique_ptr<ITexture> twoComponentTexScratch;

    std::unique_ptr<ITexture> fourComponentTex;
    std::unique_ptr<ITexture> fourComponentTexScratch;

    std::unique_ptr<ITexture> colorTex;
    std::unique_ptr<ITexture> colorTexScratch;
  };
}