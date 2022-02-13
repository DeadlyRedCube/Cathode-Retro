#include "SignalGeneration/Constants.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify
{
  ProcessContext::ProcessContext(
    GraphicsDevice *device, 
    SignalGeneration::SignalType signalTypeIn,
    uint32_t inputTextureWidth, 
    uint32_t scanlineCountIn, 
    uint32_t colorCyclesPerInputPixel, 
    uint32_t phaseGenerationDenominator)
  : signalType(signalTypeIn)
  , signalTextureWidth(int32_t(std::ceil(float(inputTextureWidth * colorCyclesPerInputPixel * NTSCify::SignalGeneration::k_signalSamplesPerColorCycle) / float(phaseGenerationDenominator))))
  , scanlineCount(scanlineCountIn)
  {
    // Create the two effectively 1D textures used to store the colorburst phases for each scanline (there are two, one for if we're just rendering
    //  a single version of the frame, and another for if we're generating two variations of the frame with a different set of phases to reduce
    //  temporal aliasing)
    scanlinePhasesOneComponent = device->CreateTexture(scanlineCount, 1, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);
    scanlinePhasesTwoComponent = device->CreateTexture(scanlineCount, 1, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);

    // Now create our one-component (float1) signal textures
    oneComponentTex = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);
    oneComponentTexScratch = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32_FLOAT, TextureFlags::RenderTarget);

    // Two-component (float2) signal textures
    twoComponentTex = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);
    twoComponentTexScratch = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32G32_FLOAT, TextureFlags::RenderTarget);

    // Four-component (float4) signal textures
    fourComponentTex = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);
    fourComponentTexScratch = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R32G32B32A32_FLOAT, TextureFlags::RenderTarget);

    // Color (32-bit RGBA) textures
    colorTex = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
    colorTexScratch = device->CreateTexture(signalTextureWidth, scanlineCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
  }
}