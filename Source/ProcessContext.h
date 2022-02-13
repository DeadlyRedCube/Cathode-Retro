#pragma once

#include "GraphicsDevice.h"
#include "SignalGeneration/SourceSettings.h"

namespace NTSCify
{
  class ProcessContext
  {
  public:
    struct TextureSetUAV
    {
      ComPtr<ID3D11Texture2D> texture;
      ComPtr<ID3D11ShaderResourceView> srv;
      ComPtr<ID3D11UnorderedAccessView> uav; // $TODO Will not need these once I'm off compute
      ComPtr<ID3D11RenderTargetView> rtv;
    };

    ProcessContext(
      GraphicsDevice *device, 
      SignalGeneration::SignalType sigalType, 
      uint32_t inputTextureWidth, 
      uint32_t scanlineCount, 
      uint32_t colorCyclesPerInputPixel, 
      uint32_t phaseGenerationDenominator);

    void RenderWithComputeShader(
      GraphicsDevice *device,
      ID3D11ComputeShader *ps,
      ID3D11Texture2D *outputTexture,
      ID3D11UnorderedAccessView *outputUav,
      std::initializer_list<ID3D11ShaderResourceView *> srvs,
      std::initializer_list<ID3D11SamplerState *> samplers,
      std::initializer_list<ID3D11Buffer *> constantBuffers);

    void RenderQuadWithPixelShader(
      GraphicsDevice *device,
      ID3D11PixelShader *ps,
      uint32_t outputTextureWidth,
      uint32_t outputTextureHeight,
      ID3D11RenderTargetView *outputRtv,
      std::initializer_list<ID3D11ShaderResourceView *> srvs,
      std::initializer_list<ID3D11SamplerState *> samplers,
      std::initializer_list<ID3D11Buffer *> constantBuffers);

    void RenderQuadWithPixelShader(
      GraphicsDevice *device,
      ID3D11PixelShader *ps,
      ID3D11Texture2D *outputTexture,
      ID3D11RenderTargetView *outputRtv,
      std::initializer_list<ID3D11ShaderResourceView *> srvs,
      std::initializer_list<ID3D11SamplerState *> samplers,
      std::initializer_list<ID3D11Buffer *> constantBuffers);

    // $TODO stop cheating and do real access control for this thing
  // private:
    SignalGeneration::SignalType signalType;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
    size_t vertexSize;
    bool hasDoubledSignal = false;

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

    TextureSetUAV scanlinePhasesOneComponent;
    TextureSetUAV scanlinePhasesTwoComponent;

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