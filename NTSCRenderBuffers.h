#pragma once

#include "GraphicsDevice.h"


class NTSCRenderBuffers
{
public:

  NTSCRenderBuffers(GraphicsDevice *device, uint32_t inputTextureWidth, uint32_t scanlineCount, uint32_t colorCyclesPerInputPixel, uint32_t phaseGenerationDenominator);

// private:
  uint32_t signalTextureWidth;
  uint32_t scanlineCount;
  size_t vertexSize;

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

  ComPtr<ID3D11Texture2D> signalTextureOneComponentA;
  ComPtr<ID3D11ShaderResourceView> signalSRVOneComponentA;
  ComPtr<ID3D11UnorderedAccessView> signalUAVOneComponentA; // $TODO May not need these once I'm off compute

  ComPtr<ID3D11Texture2D> signalTextureOneComponentB;
  ComPtr<ID3D11ShaderResourceView> signalSRVOneComponentB;
  ComPtr<ID3D11UnorderedAccessView> signalUAVOneComponentB;

  ComPtr<ID3D11Texture2D> signalTextureTwoComponentA;
  ComPtr<ID3D11ShaderResourceView> signalSRVTwoComponentA;
  ComPtr<ID3D11UnorderedAccessView> signalUAVTwoComponentA;

  ComPtr<ID3D11Texture2D> signalTextureTwoComponentB;
  ComPtr<ID3D11ShaderResourceView> signalSRVTwoComponentB;
  ComPtr<ID3D11UnorderedAccessView> signalUAVTwoComponentB;

  ComPtr<ID3D11Texture2D> signalTextureFourComponentA;
  ComPtr<ID3D11ShaderResourceView> signalSRVFourComponentA;
  ComPtr<ID3D11UnorderedAccessView> signalUAVFourComponentA;

  ComPtr<ID3D11Texture2D> signalTextureFourComponentB;
  ComPtr<ID3D11ShaderResourceView> signalSRVFourComponentB;
  ComPtr<ID3D11UnorderedAccessView> signalUAVFourComponentB;

  ComPtr<ID3D11Texture2D> signalTextureColorA;
  ComPtr<ID3D11ShaderResourceView> signalSRVColorA;
  ComPtr<ID3D11UnorderedAccessView> signalUAVColorA;

  ComPtr<ID3D11Texture2D> signalTextureColorB;
  ComPtr<ID3D11ShaderResourceView> signalSRVColorB;
  ComPtr<ID3D11UnorderedAccessView> signalUAVColorB;
};