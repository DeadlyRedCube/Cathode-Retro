#pragma once

#include <assert.h>
#include <cmath>
#include <exception>
#include <memory>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "CathodeRetro/GraphicsDevice.h"

#include "ComPtr.h"
#include "resource.h"

#define UUID_AND_ADDRESS(pObj) __uuidof(decltype(pObj.Ptr())), reinterpret_cast<void**>(pObj.AddressForReplace())

#define CHECK_HRESULT(exp, opName) \
  do \
  { \
    auto res = (exp); \
    if (FAILED(res)) \
    { \
      char exceptionStr[1024]; \
      std::snprintf(exceptionStr, sizeof(exceptionStr), "Failed to " opName ", result: %08x", uint32_t(res)); \
      throw std::runtime_error(exceptionStr); \
    } \
  } \
  while (false)


class D3DTexture : public CathodeRetro::IRenderTarget
{
public:
  uint32_t Width() const override
    { return width; }

  uint32_t Height() const override
    { return height; }

  uint32_t MipCount() const override
    { return mipCount; }

  CathodeRetro::TextureFormat Format() const override
    { return format; }

private:
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<ID3D11ShaderResourceView> fullSRV;
  std::vector<ComPtr<ID3D11ShaderResourceView>> mipSRVs;
  std::vector<ComPtr<ID3D11RenderTargetView>> mipRTVs;
  uint32_t width;
  uint32_t height;
  uint32_t mipCount;
  CathodeRetro::TextureFormat format;

  friend class D3D11GraphicsDevice;
};


class D3DPixelShader : public CathodeRetro::IShader
{
public:
  D3DPixelShader(ID3D11PixelShader *s)
    : shader(s)
    { }

private:
  ComPtr<ID3D11PixelShader> shader;

  friend class D3D11GraphicsDevice;
};


class D3DConstantBuffer : public CathodeRetro::IConstantBuffer
{
public:
  D3DConstantBuffer(ID3D11Buffer *b, ID3D11DeviceContext *ctx)
    : buffer(b)
    , context(ctx)
    { }

  void Update(const void *data, size_t dataSize) override
  {
    D3D11_MAPPED_SUBRESOURCE map;
    context->Map(buffer.Ptr(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, data, size_t(dataSize));
    context->Unmap(buffer.Ptr(), 0);
  }

private:
  ComPtr<ID3D11Buffer> buffer;
  ComPtr<ID3D11DeviceContext> context;

  friend class D3D11GraphicsDevice;
};



class D3D11GraphicsDevice : public CathodeRetro::IGraphicsDevice
{
public:
  D3D11GraphicsDevice(HWND hwnd)
  {
    window = hwnd;

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount = 2;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hwnd;
    swapDesc.Windowed = true;
    swapDesc.SampleDesc.Count = 1;

    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;

    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    UINT flags = 0;
    #if _DEBUG
      flags = D3D11_CREATE_DEVICE_DEBUG;
    #endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    CHECK_HRESULT(
      D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapDesc,
        swapChain.AddressForReplace(),
        device.AddressForReplace(),
        nullptr,
        context.AddressForReplace()),
      "create swap chain");

    ComPtr<IDXGIDevice1> dxgiDevice;
    CHECK_HRESULT(device->QueryInterface(dxgiDevice.AddressForReplace()), "get dxgi device");

    ComPtr<IDXGISwapChain2> dxgiSwapChain2;
    CHECK_HRESULT(swapChain->QueryInterface(dxgiSwapChain2.AddressForReplace()), "get DXGI Swap Chain 2");

    dxgiDevice->SetMaximumFrameLatency(1);
    dxgiSwapChain2->SetMaximumFrameLatency(1);

    swapChain->GetBuffer(0, UUID_AND_ADDRESS(backbuffer));
    CHECK_HRESULT(
      device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.AddressForReplace()),
      "create render target view");

    {
      D3D11_TEXTURE2D_DESC bbDesc;
      backbuffer->GetDesc(&bbDesc);
      backbufferWidth = bbDesc.Width;
      backbufferHeight = bbDesc.Height;
    }

    InitializeBuiltIns();
  }


  D3D11GraphicsDevice(D3D11GraphicsDevice &) = delete;
  void operator=(const D3D11GraphicsDevice &) = delete;


  void UpdateWindowSize(uint32_t width, uint32_t height)
  {
    if (width == backbufferWidth && height == backbufferHeight)
    {
      return;
    }

    backbuffer = nullptr;
    backbufferView = nullptr;

    backbufferWidth = width;
    backbufferHeight = height;

    swapChain->ResizeBuffers(
      2,
      backbufferWidth,
      backbufferHeight,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);

    swapChain->GetBuffer(0, UUID_AND_ADDRESS(backbuffer));
    CHECK_HRESULT(
      device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.AddressForReplace()),
      "create render target view");
  }

  uint32_t BackbufferWidth() const
    { return backbufferWidth; }

  uint32_t BackbufferHeight() const
    { return backbufferHeight; }

  void ClearBackbuffer()
  {
    float black[] = {0.05f, 0.05f, 0.05f, 0.05f};
    context->ClearRenderTargetView(backbufferView, black);
  }

  void Present()
  {
    assert(!isRendering);
    swapChain->Present(1, 0);
  }


  std::unique_ptr<CathodeRetro::ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    CathodeRetro::TextureFormat format,
    void *initialDataTexels)
  {
    return CreateTexture(width, height, 1, format, false, initialDataTexels);
  }


  // CathodeRetro::IGraphicsDevice Implementations ////////////////////////////////////////////////////////////////////


  std::unique_ptr<CathodeRetro::IShader> CreateShader(CathodeRetro::ShaderID id) override
  {
    assert(!isRendering);
    int resourceID = 0;
    switch (id)
    {
      case CathodeRetro::ShaderID::Util_Copy: resourceID = IDR_COPY; break;
      case CathodeRetro::ShaderID::Util_Downsample2X: resourceID = IDR_DOWNSAMPLE_2X; break;
      case CathodeRetro::ShaderID::Util_TonemapAndDownsample: resourceID = IDR_TONEMAP_AND_DOWNSAMPLE; break;
      case CathodeRetro::ShaderID::Util_GaussianBlur13: resourceID = IDR_GAUSSIAN_BLUR_13; break;
      case CathodeRetro::ShaderID::Generator_GeneratePhaseTexture: resourceID = IDR_GENERATE_PHASE_TEXTURE; break;
      case CathodeRetro::ShaderID::Generator_RGBToSVideoOrComposite: resourceID = IDR_RGB_TO_SVIDEO_OR_COMPOSITE; break;
      case CathodeRetro::ShaderID::Generator_ApplyArtifacts: resourceID = IDR_APPLY_ARTIFACTS; break;
      case CathodeRetro::ShaderID::Decoder_CompositeToSVideo: resourceID = IDR_COMPOSITE_TO_SVIDEO; break;
      case CathodeRetro::ShaderID::Decoder_SVideoToModulatedChroma: resourceID = IDR_SVIDEO_TO_MODULATED_CHROMA; break;
      case CathodeRetro::ShaderID::Decoder_SVideoToRGB: resourceID = IDR_SVIDEO_TO_RGB; break;
      case CathodeRetro::ShaderID::Decoder_FilterRGB: resourceID = IDR_FILTER_RGB; break;
      case CathodeRetro::ShaderID::CRT_GenerateScreenTexture: resourceID = IDR_GENERATE_SCREEN_TEXTURE; break;
      case CathodeRetro::ShaderID::CRT_GenerateSlotMask: resourceID = IDR_GENERATE_SLOT_MASK; break;
      case CathodeRetro::ShaderID::CRT_GenerateShadowMask: resourceID = IDR_GENERATE_SHADOW_MASK; break;
      case CathodeRetro::ShaderID::CRT_GenerateApertureGrille: resourceID = IDR_GENERATE_APERTURE_GRILLE; break;
      case CathodeRetro::ShaderID::CRT_RGBToCRT: resourceID = IDR_RGB_TO_CRT; break;
    }

    auto data = LoadResourceBytes(resourceID);
    ComPtr<ID3D11PixelShader> shader;
    CHECK_HRESULT(
      device->CreatePixelShader(
        data.data(),
        DWORD(data.size()),
        nullptr,
        shader.AddressForReplace()),
      "create pixel shader");

    return std::make_unique<D3DPixelShader>(shader.Ptr());
  }


  std::unique_ptr<CathodeRetro::IConstantBuffer> CreateConstantBuffer(size_t size) override
  {
    assert(!isRendering);
    // Constant buffers must be multiples of 16 bytes in size so round up if we're off.
    if (size & 0x0F)
    {
      size = (size & ~0x0F) + 0x10;
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = UINT(size);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> buffer;
    CHECK_HRESULT(device->CreateBuffer(&desc, nullptr, buffer.AddressForReplace()), "create constant buffer");

    return std::make_unique<D3DConstantBuffer>(buffer.Ptr(), context);
  }


  std::unique_ptr<CathodeRetro::IRenderTarget> CreateRenderTarget(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount, // 0 means "all mip levels"
    CathodeRetro::TextureFormat format) override
  {
    return std::unique_ptr<CathodeRetro::IRenderTarget>
      {
        static_cast<CathodeRetro::IRenderTarget*>(CreateTexture(
          width,
          height,
          mipCount,
          format,
          true,
          nullptr).release())
      };
  }


  void BeginRendering() override
  {
    // Any additional state to get the render state from whatever the calling app's setup is to be compatible with
    //  Cathode Retro needs to be done here.

    assert(!isRendering);
    float color[] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(blendState, color, 0xFFFFFFFF);
    context->RSSetState(rasterizerState);
    context->IASetInputLayout(inputLayout);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    isRendering = true;
  }


  void RenderQuad(
    CathodeRetro::IShader *ps,
    CathodeRetro::RenderTargetView output,
    std::initializer_list<CathodeRetro::ShaderResourceView> inputs,
    CathodeRetro::IConstantBuffer *constantBuffer = nullptr) override
  {
    uint32_t viewportWidth;
    uint32_t viewportHeight;
    ID3D11RenderTargetView *rtv;
    if (output.texture == nullptr)
    {
      // We're rendering to the backbuffer!
      viewportWidth = backbufferWidth;
      viewportHeight = backbufferHeight;
      rtv = backbufferView;
    }
    else
    {
      // Calculate the relevant mip level dimension for the target level (and generate its render target view).
      viewportWidth = std::max(1U, output.texture->Width() >> output.mipLevel);
      viewportHeight = std::max(1U, output.texture->Height() >> output.mipLevel);
      rtv = static_cast<D3DTexture *>(output.texture)->mipRTVs[output.mipLevel];
    }

    if (inputs.size() > 0)
    {
      // Set up the shaders and samplers.
      ID3D11ShaderResourceView *srvs[16] = {};
      ID3D11SamplerState *samps[16];
      for (uint32_t i = 0; i < inputs.size(); i++)
      {
        const CathodeRetro::ShaderResourceView &input = inputs.begin()[i];
        if (input.mipLevel < 0)
        {
          srvs[i] = static_cast<const D3DTexture *>(input.texture)->fullSRV;
        }
        else
        {
          srvs[i] = static_cast<const D3DTexture *>(input.texture)->mipSRVs[input.mipLevel];
        }

        samps[i] = samplerStates[uint32_t(input.samplerType)];
      }

      context->PSSetShaderResources(0, UINT(inputs.size()), srvs);
      context->PSSetSamplers(0, UINT(inputs.size()), samps);
    }

    assert(isRendering);

    // Set up the render target and viewport
    {
      context->OMSetRenderTargets(1, &rtv, nullptr);

      D3D11_VIEWPORT vp;
      vp.TopLeftX = 0.0f;
      vp.TopLeftY = 0.0f;
      vp.Width = float(viewportWidth);
      vp.Height = float(viewportHeight);
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;

      context->RSSetViewports(1, &vp);
    }

    context->PSSetShader(static_cast<D3DPixelShader *>(ps)->shader, nullptr, 0);

    {
      auto ptr = vertexBuffer.Ptr();
      UINT stride = UINT(sizeof(Vertex));
      UINT offsets = 0;
      context->IASetVertexBuffers(0, 1, &ptr, &stride, &offsets);
    }

    if (constantBuffer != nullptr)
    {
      auto cb = static_cast<D3DConstantBuffer *>(constantBuffer)->buffer.Ptr();
      context->PSSetConstantBuffers(0, 1, &cb);
    }

    context->Draw(6, 0);

    // Clear our shader resources and render target once we're done.
    if (inputs.size() > 0)
    {
      ID3D11ShaderResourceView *nullSrvs[16] = {};
      context->PSSetShaderResources(0, UINT(inputs.size()), nullSrvs);
    }

    rtv = nullptr;
    context->OMSetRenderTargets(1, &rtv, nullptr);
  }


  void EndRendering() override
  {
    assert(isRendering);
    ID3D11Buffer *cb = nullptr;
    context->PSSetConstantBuffers(0, 1, &cb);
    isRendering = false;

    // Can now restore any D3D state for the calling app, as necessary.
  }


private:
  struct Vertex
  {
    float x, y;
  };

  std::unique_ptr<CathodeRetro::ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount,
    CathodeRetro::TextureFormat format,
    bool isRenderTarget,
    void *initialDataTexels)
  {
    assert(!isRendering);
    std::unique_ptr<D3DTexture> tex = std::make_unique<D3DTexture>();

    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t texelByteCount = 0;
    switch (format)
    {
    case CathodeRetro::TextureFormat::RGBA_Unorm8:
      dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
      texelByteCount = 4 * sizeof(uint8_t);
      break;

    case CathodeRetro::TextureFormat::R_Float32:
      dxgiFormat = DXGI_FORMAT_R32_FLOAT;
      texelByteCount = 1 * sizeof(float);
      break;

    case CathodeRetro::TextureFormat::RG_Float32:
      dxgiFormat = DXGI_FORMAT_R32G32_FLOAT;
      texelByteCount = 2 * sizeof(float);
      break;

    case CathodeRetro::TextureFormat::RGBA_Float32:
      dxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
      texelByteCount = 4 * sizeof(float);
      break;
    }

    {
      D3D11_TEXTURE2D_DESC desc = {};
      desc.Width = width;
      desc.Height = height;
      desc.ArraySize = 1;
      desc.Format = dxgiFormat;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.CPUAccessFlags = (isRenderTarget) ? D3D11_CPU_ACCESS_WRITE : 0;
      desc.MipLevels = mipCount;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      if (isRenderTarget)
      {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
      }

      D3D11_SUBRESOURCE_DATA initialDataStorage = {};
      D3D11_SUBRESOURCE_DATA *initialData = nullptr;
      if (initialDataTexels != nullptr)
      {
        initialData = &initialDataStorage;

        initialData->pSysMem = initialDataTexels;
        initialData->SysMemPitch = width * texelByteCount;
      }

      tex->width = width;
      tex->height = height;
      tex->format = format;

      CHECK_HRESULT(
        device->CreateTexture2D(&desc, initialData, tex->texture.AddressForReplace()),
        "create texture 2D");
      CHECK_HRESULT(
        device->CreateShaderResourceView(tex->texture, nullptr, tex->fullSRV.AddressForReplace()),
        "create SRV");

      tex->texture->GetDesc(&desc);
      tex->mipCount = desc.MipLevels;
    }

    for (uint32_t mip = 0; mip < tex->mipCount; mip++)
    {
      {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = dxgiFormat;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = mip;

        ComPtr<ID3D11ShaderResourceView> srv;
        CHECK_HRESULT(device->CreateShaderResourceView(tex->texture, &desc, srv.AddressForReplace()), "Create SRV");
        tex->mipSRVs.push_back(std::move(srv));
      }

      if (isRenderTarget)
      {
        D3D11_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = dxgiFormat;
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = mip;

        ComPtr<ID3D11RenderTargetView> rtv;
        CHECK_HRESULT(device->CreateRenderTargetView(tex->texture, &desc, rtv.AddressForReplace()), "Create RTV");
        tex->mipRTVs.push_back(std::move(rtv));
      }
    }

    return tex;
  }


  // Load a resource from the current executable. Used because I packed the shaders into the RC like a weirdo.
  static std::vector<uint8_t> LoadResourceBytes(int resourceId)
  {
    HRSRC resource = FindResourceW(GetModuleHandle(nullptr), MAKEINTRESOURCE(resourceId), L"RT_RCDATA");
    if (resource == nullptr)
    {
      throw std::runtime_error("Failed to find resource");
    }

    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (loaded == nullptr)
    {
      throw std::runtime_error("Failed to load resource");
    }

    void *resData = LockResource(loaded);
    DWORD resSize = SizeofResource(0, resource);

    std::vector<uint8_t> bytes;
    bytes.resize(size_t(resSize));
    memcpy(bytes.data(), resData, bytes.size());
    return bytes;
  }


  void InitializeBuiltIns()
  {
    // Create the basic vertex buffer (just a square made of 6 vertices, it wasn't even worth dealing with an index
    //  buffer for a single quad)
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

      D3D11_BUFFER_DESC vbDesc = {};
      vbDesc.ByteWidth = sizeof(data);
      vbDesc.Usage = D3D11_USAGE_DEFAULT;
      vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

      D3D11_SUBRESOURCE_DATA initial;
      initial.pSysMem = data;
      initial.SysMemPitch = 0;
      initial.SysMemSlicePitch = 0;

      CHECK_HRESULT(device->CreateBuffer(&vbDesc, &initial, vertexBuffer.AddressForReplace()), "create vertex buffer");
    }

    // Load our basic vertex shader, which can be reused for multiple pixel shaders
    {
      D3D11_INPUT_ELEMENT_DESC elements[] =
      {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA , 0},
      };

      auto data = LoadResourceBytes(IDR_BASIC_VERTEX_SHADER);
      CHECK_HRESULT(
        device->CreateVertexShader(
          data.data(),
          DWORD(data.size()),
          nullptr,
          vertexShader.AddressForReplace()),
        "create vertex shader");

      CHECK_HRESULT(
        device->CreateInputLayout(
          elements,
          1,
          data.data(),
          DWORD(data.size()),
          inputLayout.AddressForReplace()),
        "create input layout");
    }

    // Samplers!
    {
      D3D11_SAMPLER_DESC desc = {};
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      desc.MaxAnisotropy = 16;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(
        device->CreateSamplerState(
          &desc,
          samplerStates[uint32_t(CathodeRetro::SamplerType::LinearClamp)].AddressForReplace()),
        "create standard sampler state");
    }

    {
      D3D11_SAMPLER_DESC desc = {};
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      desc.MaxAnisotropy = 16;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(
        device->CreateSamplerState(
          &desc,
          samplerStates[uint32_t(CathodeRetro::SamplerType::LinearWrap)].AddressForReplace()),
        "create wrap sampler state");
    }

    {
      D3D11_SAMPLER_DESC desc = {};
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
      desc.MaxAnisotropy = 16;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(
        device->CreateSamplerState(
          &desc,
          samplerStates[uint32_t(CathodeRetro::SamplerType::NearestClamp)].AddressForReplace()),
        "create standard sampler state");
    }

    {
      D3D11_SAMPLER_DESC desc = {};
      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
      desc.MaxAnisotropy = 16;
      desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      desc.MinLOD = 0;
      desc.MaxLOD = D3D11_FLOAT32_MAX;
      CHECK_HRESULT(
        device->CreateSamplerState(
          &desc,
          samplerStates[uint32_t(CathodeRetro::SamplerType::NearestWrap)].AddressForReplace()),
        "create wrap sampler state");
    }

    // Rasterizer/blend states!
    {
      D3D11_RASTERIZER_DESC desc = {};
      desc.FillMode = D3D11_FILL_SOLID;
      desc.CullMode = D3D11_CULL_NONE;
      CHECK_HRESULT(
        device->CreateRasterizerState(&desc, rasterizerState.AddressForReplace()),
        "create rasterizer state");
    }

    {
      D3D11_BLEND_DESC desc = {};
      desc.RenderTarget[0].BlendEnable = false;
      desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      CHECK_HRESULT(device->CreateBlendState(&desc, blendState.AddressForReplace()), "create blend state");
    }
  }

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain> swapChain;
  ComPtr<ID3D11Texture2D> backbuffer;
  ComPtr<ID3D11RenderTargetView> backbufferView;
  uint32_t backbufferWidth;
  uint32_t backbufferHeight;

  ComPtr<ID3D11Buffer> vertexBuffer;
  ComPtr<ID3D11InputLayout> inputLayout;

  ComPtr<ID3D11SamplerState> samplerStates[4];
  ComPtr<ID3D11RasterizerState> rasterizerState;
  ComPtr<ID3D11BlendState> blendState;

  ComPtr<ID3D11VertexShader> vertexShader;

  HWND window;
  uint32_t prevSamplerCount = 0;
  bool isRendering = false;
};


#undef CHECK_HRESULT
#undef UUID_AND_ADDRESS