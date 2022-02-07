#include "SignalGeneration/RGBToSVideo.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"


namespace NTSCify
{
  struct Vertex
  {
    float x, y;
  };


  ProcessContext::ProcessContext(
    GraphicsDevice *device, 
    uint32_t inputTextureWidth, 
    uint32_t scanlineCountIn, 
    uint32_t colorCyclesPerInputPixel, 
    uint32_t phaseGenerationDenominator)
  : signalTextureWidth(int32_t(std::ceil(float(inputTextureWidth * colorCyclesPerInputPixel * NTSCify::SignalGeneration::k_signalSamplesPerColorCycle) / float(phaseGenerationDenominator))))
  , scanlineCount(scanlineCountIn)
  , vertexSize(sizeof(Vertex))
  {
    {
      Vertex data[]
      {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
      };

      D3D11_BUFFER_DESC vbDesc;
      ZeroType(&vbDesc);
      vbDesc.ByteWidth = sizeof(data);
      vbDesc.Usage = D3D11_USAGE_DEFAULT;
      vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
      D3D11_SUBRESOURCE_DATA initial;
      initial.pSysMem = data;
      initial.SysMemPitch = 0;
      initial.SysMemSlicePitch = 0;

      CHECK_HRESULT(device->D3DDevice()->CreateBuffer(&vbDesc, &initial, vertexBuffer.AddressForReplace()), "create vertex buffer");
    }
  
    {
      D3D11_INPUT_ELEMENT_DESC elements[] =
      { 
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA , 0},
      };

      device->CreateVertexShaderAndInputLayout(
        IDR_BASICVERTEXSHADER, 
        elements,
        k_arrayLength<decltype(elements)>,
        &vertexShader, 
        &inputLayout);
    }

    {
      D3D11_SAMPLER_DESC desc;
      ZeroType(&desc);
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MipLODBias = 0.25f;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(device->D3DDevice()->CreateSamplerState(&desc, samplerStateClamp.AddressForReplace()), "create standard sampler state");
    }

    {
      D3D11_SAMPLER_DESC desc;
      ZeroType(&desc);
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      desc.MaxAnisotropy = 16;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MipLODBias = 0.25f;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(device->D3DDevice()->CreateSamplerState(&desc, samplerStateWrap.AddressForReplace()), "create wrap sampler state");
    }

    {
      D3D11_RASTERIZER_DESC desc;
      ZeroType(&desc);
      desc.FillMode = D3D11_FILL_SOLID;
      desc.CullMode = D3D11_CULL_NONE;
      CHECK_HRESULT(device->D3DDevice()->CreateRasterizerState(&desc, rasterizerState.AddressForReplace()), "create rasterizer state");
    }

    {
      D3D11_BLEND_DESC desc;
      ZeroType(&desc);
      desc.RenderTarget[0].BlendEnable = false;
      desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      CHECK_HRESULT(device->D3DDevice()->CreateBlendState(&desc, blendState.AddressForReplace()), "create blend state");
    }

    device->CreateTexture2D(
      scanlineCount,
      1,
      DXGI_FORMAT_R32_FLOAT,
      GraphicsDevice::TextureFlags::Dynamic,
      &scanlinePhasesTexture,
      &scanlinePhasesSRV);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &oneComponentTexA.texture,
      &oneComponentTexA.srv,
      &oneComponentTexA.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &oneComponentTexB.texture,
      &oneComponentTexB.srv,
      &oneComponentTexB.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32G32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &twoComponentTexA.texture,
      &twoComponentTexA.srv,
      &twoComponentTexA.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32G32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &twoComponentTexB.texture,
      &twoComponentTexB.srv,
      &twoComponentTexB.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &fourComponentTexA.texture,
      &fourComponentTexA.srv,
      &fourComponentTexA.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      GraphicsDevice::TextureFlags::UAV,
      &fourComponentTexB.texture,
      &fourComponentTexB.srv,
      &fourComponentTexB.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      GraphicsDevice::TextureFlags::UAV,
      &colorTexA.texture,
      &colorTexA.srv,
      &colorTexA.uav);

    device->CreateTexture2D(
      signalTextureWidth,
      scanlineCount,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      GraphicsDevice::TextureFlags::UAV,
      &colorTexB.texture,
      &colorTexB.srv,
      &colorTexB.uav);
  }
}