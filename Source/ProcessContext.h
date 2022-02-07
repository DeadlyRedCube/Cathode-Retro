#pragma once

#include "GraphicsDevice.h"
#include "SignalGeneration/SourceSettings.h"

namespace NTSCify
{
  class ProcessContext
  {
  public:
     // $TODO May not need these once I'm off compute
    struct TextureSetUAV
    {
      ComPtr<ID3D11Texture2D> texture;
      ComPtr<ID3D11ShaderResourceView> srv;
      ComPtr<ID3D11UnorderedAccessView> uav;
    };


    ProcessContext(GraphicsDevice *device, SignalGeneration::SignalType sigalType, uint32_t inputTextureWidth, uint32_t scanlineCount, uint32_t colorCyclesPerInputPixel, uint32_t phaseGenerationDenominator);

  // private:
    SignalGeneration::SignalType signalType;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
    size_t vertexSize;

    // Set by signal generation
    float whiteLevel;
    float blackLevel;
    float saturationScale;

    // All these shaders use the same vertex buffer and input layout.
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11InputLayout> inputLayout;

    ComPtr<ID3D11SamplerState> samplerStateClamp;
    ComPtr<ID3D11SamplerState> samplerStateWrap;
    ComPtr<ID3D11RasterizerState> rasterizerState;
    ComPtr<ID3D11BlendState> blendState;

    ComPtr<ID3D11VertexShader> vertexShader;

    ComPtr<ID3D11Texture2D> scanlinePhasesTexture;
    ComPtr<ID3D11ShaderResourceView> scanlinePhasesSRV;

    TextureSetUAV oneComponentTex;
    TextureSetUAV oneComponentTexScratch;

    TextureSetUAV twoComponentTex;
    TextureSetUAV twoComponentTexScratch;

    TextureSetUAV fourComponentTex;
    TextureSetUAV fourComponentTexScratch;

    TextureSetUAV colorTex;
    TextureSetUAV colorTexScratch;
  };
}