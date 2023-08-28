#define NOMINMAX

#include <d3d11.h>
#include <Windows.h>
#include <dxgi1_3.h>

#include <cmath>
#include <exception>
#include <memory>
#include <vector>

#include "ComPtr.h"
#include "D3D11GraphicsDevice.h"
#include "resource.h"
#include "Util.h"

#define UUID_AND_ADDRESS(pObj) __uuidof(decltype(pObj.Ptr())), reinterpret_cast<void**>(pObj.AddressForReplace())

#define CHECK_HRESULT(exp, opName) \
  do \
  { \
    if (auto res = (exp); FAILED(res)) \
    { \
      char exceptionStr[1024]; \
      sprintf_s(exceptionStr, "Failed to " opName ", result: %08x", uint32_t(res)); \
      throw std::exception(exceptionStr); \
    } \
  } \
  while (false)


#define DEBUG_DEVICE 1

struct Vertex
{
  float x, y;
};
    

    // A rather minimal wrapper around a D3D device and related functionality
class D3D11GraphicsDevice : public ID3D11GraphicsDevice
{
public:
  D3D11GraphicsDevice(HWND hwnd);
  
  D3D11GraphicsDevice(D3D11GraphicsDevice &) = delete;
  void operator=(const D3D11GraphicsDevice &) = delete;

  void UpdateWindowSize() override;

  uint32_t OutputWidth() const override
    { return backbufferWidth; }

  uint32_t OutputHeight() const override
    { return backbufferHeight; }

  void ClearBackbuffer() override;

  void Present() override;

  std::unique_ptr<IShader> CreateShader(ShaderID id) override;
  std::unique_ptr<IConstantBuffer> CreateConstantBuffer(size_t size) override;

  void DiscardAndUpdateBuffer(IConstantBuffer *buffer, const void *data, size_t dataSize) override;

  std::vector<uint32_t> GetTexturePixels(ITexture *texture);

  std::unique_ptr<ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount,
    TextureFormat format,
    TextureFlags flags,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0);


  void RenderQuad(
    IShader *ps,
    RenderTargetView output,
    std::initializer_list<ShaderResourceView> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<IConstantBuffer *> constantBuffers) override;

private:
  class Texture : public ITexture
  {
  public:
    uint32_t Width() const override
      { return width; }

    uint32_t Height() const override
      { return height; }

    uint32_t MipCount() const override
      { return mipCount; }

    TextureFormat Format() const override
      { return format; }

    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> fullSRV;
    std::vector<ComPtr<ID3D11ShaderResourceView>> mipSRVs;
    std::vector<ComPtr<ID3D11RenderTargetView>> mipRTVs;
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
    TextureFormat format;
  };


  class PixelShader : public IShader
  {
  public:
    PixelShader(ID3D11PixelShader *s)
      : shader(s)
      { }

    ComPtr<ID3D11PixelShader> shader;
  };


  class ConstantBuffer : public IConstantBuffer
  {
  public:
    ConstantBuffer(ID3D11Buffer *b)
      : buffer(b)
      { }

    ComPtr<ID3D11Buffer> buffer;
  };


  void RenderQuad(
    IShader *ps,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    ID3D11RenderTargetView *outputRtv,
    std::initializer_list<ShaderResourceView> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<IConstantBuffer *> constantBuffers);
    
  void RenderQuad(
    IShader *ps,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    ID3D11RenderTargetView *outputRtv,
    ID3D11ShaderResourceView **srvs,
    uint32_t srvCount,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<IConstantBuffer *> constantBuffers);
    
  void CreateVertexShaderAndInputLayout(
    int resourceID, 
    D3D11_INPUT_ELEMENT_DESC *layoutElements,
    size_t layoutElementCount,
    ComPtr<ID3D11VertexShader> *shaderOut, 
    ComPtr<ID3D11InputLayout> *layoutOut);
  
  void InitializeBuiltIns();

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain> swapChain;
  ComPtr<ID3D11Texture2D> backbuffer;
  ComPtr<ID3D11RenderTargetView> backbufferView;
  uint32_t backbufferWidth;
  uint32_t backbufferHeight;

  ComPtr<ID3D11Buffer> vertexBuffer;
  ComPtr<ID3D11InputLayout> inputLayout;

  ComPtr<ID3D11SamplerState> samplerStates[2];
  ComPtr<ID3D11RasterizerState> rasterizerState;
  ComPtr<ID3D11BlendState> blendState;

  ComPtr<ID3D11VertexShader> vertexShader;

  HWND window;
};
// Load a resource from the current executable. Used because I packed the shaders into the RC like a weirdo
static std::vector<uint8_t> LoadResourceBytes(int resourceId)
{
  HRSRC resource = FindResourceW(GetModuleHandle(nullptr), MAKEINTRESOURCE(resourceId), L"RT_RCDATA");
  if (resource == nullptr)
  {
    throw std::exception("Failed to find resource");
  }

  HGLOBAL loaded = LoadResource(nullptr, resource);
  if (loaded == nullptr)
  {
    throw std::exception("Failed to load resource");
  }

  void *resData = LockResource(loaded);
  DWORD resSize = SizeofResource(0, resource);

  std::vector<uint8_t> bytes;
  bytes.resize(size_t(resSize));
  memcpy(bytes.data(), resData, bytes.size());
  return bytes;
}


void D3D11GraphicsDevice::ClearBackbuffer()
{
  float black[] = {0.05f, 0.05f, 0.05f, 0.05f};
  context->ClearRenderTargetView(backbufferView, black);
}


void D3D11GraphicsDevice::CreateVertexShaderAndInputLayout(
  int resourceID, 
  D3D11_INPUT_ELEMENT_DESC *layoutElements,
  size_t layoutElementCount,
  ComPtr<ID3D11VertexShader> *shaderOut, 
  ComPtr<ID3D11InputLayout> *layoutOut)
{
  auto data = LoadResourceBytes(resourceID);
  CHECK_HRESULT(
    device->CreateVertexShader(
      data.data(),
      DWORD(data.size()),
      nullptr, 
      shaderOut->AddressForReplace()), 
    "create vertex shader");

  CHECK_HRESULT(
    device->CreateInputLayout(
      layoutElements,
      DWORD(layoutElementCount),
      data.data(),
      DWORD(data.size()),
      layoutOut->AddressForReplace()),
    "create input layout");
}

  
std::unique_ptr<IShader> D3D11GraphicsDevice::CreateShader(ShaderID id)
{
  int resourceID = 0;
  switch (id)
  {
    case ShaderID::Downsample2X: resourceID = IDR_DOWNSAMPLE_2X; break;
    case ShaderID::GeneratePhaseTexture: resourceID = IDR_GENERATE_PHASE_TEXTURE; break;
    case ShaderID::RGBToSVideoOrComposite: resourceID = IDR_RGB_TO_SVIDEO_OR_COMPOSITE; break;
    case ShaderID::ApplyArtifacts: resourceID = IDR_APPLY_ARTIFACTS; break;
    case ShaderID::CompositeToSVideo: resourceID = IDR_COMPOSITE_TO_SVIDEO; break;
    case ShaderID::SVideoToYIQ: resourceID = IDR_SVIDEO_TO_YIQ; break;
    case ShaderID::YIQToRGB: resourceID = IDR_YIQ_TO_RGB; break;
    case ShaderID::FilterRGB: resourceID = IDR_FILTER_RGB; break;
    case ShaderID::GenerateScreenTexture: resourceID = IDR_GENERATE_SCREEN_TEXTURE; break;
    case ShaderID::GenerateShadowMask: resourceID = IDR_GENERATE_SHADOW_MASK; break;
    case ShaderID::TonemapAndDownsample: resourceID = IDR_TONEMAP_AND_DOWNSAMPLE; break;
    case ShaderID::GaussianBlur13: resourceID = IDR_GAUSSIAN_BLUR_13; break;
    case ShaderID::RGBToCRT: resourceID = IDR_RGB_TO_CRT; break;
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

  return std::make_unique<PixelShader>(shader.Ptr());
}

  
std::unique_ptr<ITexture> D3D11GraphicsDevice::CreateTexture(
  uint32_t width,
  uint32_t height,
  uint32_t mipCount,
  TextureFormat format,
  TextureFlags flags,
  void *initialDataTexels,
  uint32_t initialDataPitch)
{
  std::unique_ptr<Texture> tex = std::make_unique<Texture>();

  DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  switch (format)
  {
  case TextureFormat::RGBA_Unorm8:
    dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    break;

  case TextureFormat::R_Float32:
    dxgiFormat = DXGI_FORMAT_R32_FLOAT;
    break;

  case TextureFormat::RG_Float32:
    dxgiFormat = DXGI_FORMAT_R32G32_FLOAT;
    break;

  case TextureFormat::RGBA_Float32:
    dxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    break;
  }


  {
    D3D11_TEXTURE2D_DESC desc;
    ZeroType(&desc);
    desc.Width = width;
    desc.Height = height;
    desc.ArraySize = 1;
    desc.Format = dxgiFormat;
    desc.SampleDesc.Count = 1;
    desc.Usage = ((flags & TextureFlags::Dynamic) != TextureFlags::None) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = ((flags & TextureFlags::Dynamic) != TextureFlags::None) ? D3D11_CPU_ACCESS_WRITE : 0;
    desc.MipLevels = mipCount;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (((flags & TextureFlags::RenderTarget) != TextureFlags::None))
    {
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    D3D11_SUBRESOURCE_DATA initialDataStorage;
    D3D11_SUBRESOURCE_DATA *initialData = nullptr;
    if (initialDataTexels != nullptr)
    {
      ZeroType(&initialDataStorage);
      initialData = &initialDataStorage;

      initialData->pSysMem = initialDataTexels;
      initialData->SysMemPitch = initialDataPitch;
    }

    tex->width = width;
    tex->height = height;
    tex->format = format;

    CHECK_HRESULT(device->CreateTexture2D(&desc, initialData, tex->texture.AddressForReplace()), "create texture 2D");
    CHECK_HRESULT(device->CreateShaderResourceView(tex->texture, nullptr, tex->fullSRV.AddressForReplace()), "create SRV");

    tex->texture->GetDesc(&desc);
    tex->mipCount = desc.MipLevels;
  }

  for (uint32_t mip = 0; mip < tex->mipCount; mip++)
  {
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC desc;
      ZeroType(&desc);
      desc.Format = dxgiFormat;
      desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipLevels = 1;
      desc.Texture2D.MostDetailedMip = mip;

      ComPtr<ID3D11ShaderResourceView> srv;
      CHECK_HRESULT(device->CreateShaderResourceView(tex->texture, &desc, srv.AddressForReplace()), "Create SRV");
      tex->mipSRVs.push_back(std::move(srv));
    }

    if (((flags & TextureFlags::RenderTarget) != TextureFlags::None))
    {
      D3D11_RENDER_TARGET_VIEW_DESC desc;
      ZeroType(&desc);
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


std::vector<uint32_t> D3D11GraphicsDevice::GetTexturePixels(ITexture *texture)
{
  auto tex = static_cast<Texture *>(texture);

  D3D11_TEXTURE2D_DESC desc;
  tex->texture->GetDesc(&desc);

  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.MiscFlags = 0;

  ComPtr<ID3D11Texture2D> staging;
  CHECK_HRESULT(device->CreateTexture2D(&desc, nullptr, staging.AddressForReplace()), "create copy texture");

  context->CopyResource(staging.Ptr(), tex->texture);
  
  D3D11_MAPPED_SUBRESOURCE resource;
  CHECK_HRESULT(context->Map(staging.Ptr(), 0, D3D11_MAP_READ, 0, &resource), "mapping staging texture");

  std::vector<uint32_t> ary;
  ary.resize(tex->Width() * tex->Height());

  for (uint32_t y = 0; y < tex->Height(); y++)
  {
    memcpy(&ary[y * tex->Width()], resource.pData, tex->Width()*sizeof(uint32_t));
    resource.pData = static_cast<uint8_t *>(resource.pData) + resource.RowPitch;
  }

  context->Unmap(staging.Ptr(), 0);
  return ary;
}


std::unique_ptr<IConstantBuffer> D3D11GraphicsDevice::CreateConstantBuffer(size_t size)
{
  // Constant buffers must be multiples of 16 bytes in size so round up if we're off.
  if (size & 0x0F)
  {
    size = (size & ~0x0F) + 0x10;
  }

  D3D11_BUFFER_DESC desc;
  ZeroType(&desc);
  desc.ByteWidth = UINT(size);
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ComPtr<ID3D11Buffer> buffer;
  CHECK_HRESULT(device->CreateBuffer(&desc, nullptr, buffer.AddressForReplace()), "create constant buffer");

  return std::make_unique<ConstantBuffer>(buffer.Ptr());
}


void D3D11GraphicsDevice::DiscardAndUpdateBuffer(IConstantBuffer *bufferIn, const void *data, size_t dataSize)
{
  D3D11_MAPPED_SUBRESOURCE map;
  ID3D11Buffer *buffer = static_cast<ConstantBuffer *>(bufferIn)->buffer.Ptr();
  context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  memcpy(map.pData, data, size_t(dataSize));
  context->Unmap(buffer, 0);
}


D3D11GraphicsDevice::D3D11GraphicsDevice(HWND hwnd)
{
  window = hwnd;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  ZeroType(&swapDesc);
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
#if DEBUG_DEVICE
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
  CHECK_HRESULT(device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.AddressForReplace()), "create render target view");

  {
    D3D11_TEXTURE2D_DESC bbDesc;
    backbuffer->GetDesc(&bbDesc);
    backbufferWidth = bbDesc.Width;
    backbufferHeight = bbDesc.Height;
  }

  InitializeBuiltIns();
}


void D3D11GraphicsDevice::InitializeBuiltIns()
{

  // Create the basic vertex buffer (just a square made of 6 vertices because it wasn't even worth dealing with an index buffer for a single quad)
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

    CHECK_HRESULT(device->CreateBuffer(&vbDesc, &initial, vertexBuffer.AddressForReplace()), "create vertex buffer");
  }
  
  // Load our basic vertex shader, which can be reused for multiple pixel shaders
  {
    D3D11_INPUT_ELEMENT_DESC elements[] =
    { 
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA , 0},
    };

    CreateVertexShaderAndInputLayout(
      IDR_BASIC_VERTEX_SHADER, 
      elements,
      k_arrayLength<decltype(elements)>,
      &vertexShader, 
      &inputLayout);
  }

  // Samplers!
  {
    D3D11_SAMPLER_DESC desc;
    ZeroType(&desc);
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.MaxAnisotropy = 16;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    CHECK_HRESULT(device->CreateSamplerState(&desc, samplerStates[EnumValue(SamplerType::LinearClamp)].AddressForReplace()), "create standard sampler state");
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
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    CHECK_HRESULT(device->CreateSamplerState(&desc, samplerStates[EnumValue(SamplerType::LinearWrap)].AddressForReplace()), "create wrap sampler state");
  }

  // Rasterizer/blend states!
  {
    D3D11_RASTERIZER_DESC desc;
    ZeroType(&desc);
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;
    CHECK_HRESULT(device->CreateRasterizerState(&desc, rasterizerState.AddressForReplace()), "create rasterizer state");
  }

  {
    D3D11_BLEND_DESC desc;
    ZeroType(&desc);
    desc.RenderTarget[0].BlendEnable = false;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    CHECK_HRESULT(device->CreateBlendState(&desc, blendState.AddressForReplace()), "create blend state");
  }
}


void D3D11GraphicsDevice::UpdateWindowSize()
{
  RECT clientRect;
  GetClientRect(window, &clientRect);
  uint32_t newWidth = clientRect.right - clientRect.left;
  uint32_t newHeight = clientRect.bottom - clientRect.top;

  if (newWidth == backbufferWidth && newHeight == backbufferHeight)
  {
    return;
  }

  backbuffer = nullptr;
  backbufferView = nullptr;

  backbufferWidth = newWidth;
  backbufferHeight = newHeight;

  swapChain->ResizeBuffers(
    2, 
    backbufferWidth,
    backbufferHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);

  swapChain->GetBuffer(0, UUID_AND_ADDRESS(backbuffer));
  CHECK_HRESULT(device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.AddressForReplace()), "create render target view");

}


void D3D11GraphicsDevice::Present()
{
  swapChain->Present(1, 0);
}


void D3D11GraphicsDevice::RenderQuad(
  IShader *ps,
  RenderTargetView output,
  std::initializer_list<ShaderResourceView> inputs,
  std::initializer_list<SamplerType> samplers,
  std::initializer_list<IConstantBuffer *> constantBuffers)
{
  if (output.texture == nullptr)
  {
    RenderQuad(
      ps,
      backbufferWidth,
      backbufferHeight,
      backbufferView,
      inputs,
      samplers,
      constantBuffers);
  }
  else
  {
    RenderQuad(
      ps,
      std::max(1U, output.texture->Width() >> output.mipLevel),
      std::max(1U, output.texture->Height() >> output.mipLevel),
      static_cast<Texture *>(output.texture)->mipRTVs[output.mipLevel],
      inputs,
      samplers,
      constantBuffers);
  }
}


void D3D11GraphicsDevice::RenderQuad(
  IShader *ps,
  uint32_t viewportWidth,
  uint32_t viewportHeight,
  ID3D11RenderTargetView *outputRtv,
  std::initializer_list<ShaderResourceView> inputs,
  std::initializer_list<SamplerType> samplers,
  std::initializer_list<IConstantBuffer *> constantBuffers)
{
  ID3D11ShaderResourceView *srvs[16] = {};
  for (uint32_t i = 0; i < inputs.size(); i++)
  {
    const ShaderResourceView &input = inputs.begin()[i];
    if (input.mipLevel < 0)
    {
      srvs[i] = static_cast<const Texture *>(input.texture)->fullSRV;
    }
    else
    {
      srvs[i] = static_cast<const Texture *>(input.texture)->mipSRVs[input.mipLevel];
    }
  }

  RenderQuad(
    ps,
    viewportWidth,
    viewportHeight,
    outputRtv,
    srvs,
    uint32_t(inputs.size()),
    samplers,
    constantBuffers);
}


void D3D11GraphicsDevice::RenderQuad(
  IShader *ps,
  uint32_t viewportWidth,
  uint32_t viewportHeight,
  ID3D11RenderTargetView *outputRtv,
  ID3D11ShaderResourceView **srvs,
  uint32_t srvCount,
  std::initializer_list<SamplerType> samplers,
  std::initializer_list<IConstantBuffer *> constantBuffers)
{
  {
    context->OMSetRenderTargets(1, &outputRtv, nullptr);

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = float(viewportWidth);
    vp.Height = float(viewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    context->RSSetViewports(1, &vp);
  }

  float color[] = {0.0f, 0.0f, 0.0f, 0.0f};
  context->OMSetBlendState(blendState, color, 0xFFFFFFFF);
  context->RSSetState(rasterizerState);
  context->IASetInputLayout(inputLayout);
  context->VSSetShader(vertexShader, nullptr, 0);
  context->PSSetShader(static_cast<PixelShader *>(ps)->shader, nullptr, 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  {
    auto ptr = vertexBuffer.Ptr();
    UINT stride = UINT(sizeof(Vertex));
    UINT offsets = 0;
    context->IASetVertexBuffers(0, 1, &ptr, &stride, &offsets);
  }

  if (samplers.size() > 0)
  {
    ID3D11SamplerState *samps[16];

    for (uint32_t i = 0; i < samplers.size(); i++)
    {
      samps[i] = samplerStates[EnumValue(samplers.begin()[i])];
    }
    
    context->PSSetSamplers(0, UINT(samplers.size()), samps);
  }
    
  if (constantBuffers.size() > 0)
  {
    ID3D11Buffer *cbs[16];
    for (uint32_t i = 0; i < constantBuffers.size(); i++)
    {
      cbs[i] = static_cast<ConstantBuffer *>(constantBuffers.begin()[i])->buffer;
    }
    
    context->PSSetConstantBuffers(0, UINT(constantBuffers.size()), cbs);
  }

  if (srvCount > 0)
  {
    context->PSSetShaderResources(0, UINT(srvCount), srvs);
  }
    
  context->Draw(6, 0);
    
  if (srvCount)
  {
    ID3D11ShaderResourceView *nullSrvs[16] = {};
    context->PSSetShaderResources(0, UINT(srvCount), nullSrvs);
  }

  outputRtv = nullptr;
  context->OMSetRenderTargets(1, &outputRtv, nullptr);
}


std::unique_ptr<ID3D11GraphicsDevice> ID3D11GraphicsDevice::Create(HWND hwnd)
{
  return std::make_unique<D3D11GraphicsDevice>(hwnd);
}