#include <dxgi1_3.h>
#include <cmath>
#include <exception>
#include <stdio.h>
#include <vector>
#include "GraphicsDevice.h"
#include "Util.h"

#define DEBUG_DEVICE 1


// Load a resource from the current executable. Used because I packed the shaders into the RC like a weirdo
static SimpleArray<uint8_t> LoadResourceBytes(int resourceId)
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

  SimpleArray<uint8_t> bytes{size_t(resSize)};
  memcpy(bytes.Ptr(), resData, bytes.Length());
  return bytes;
}


void GraphicsDevice::ClearBackbuffer()
{
  float black[] = {0.05f, 0.05f, 0.05f, 0.05f};
  context->ClearRenderTargetView(backbufferView, black);
}


void GraphicsDevice::CreateVertexShaderAndInputLayout(
  int resourceID, 
  D3D11_INPUT_ELEMENT_DESC *layoutElements,
  size_t layoutElementCount,
  ComPtr<ID3D11VertexShader> *shaderOut, 
  ComPtr<ID3D11InputLayout> *layoutOut)
{
  auto data = LoadResourceBytes(resourceID);
  CHECK_HRESULT(
    device->CreateVertexShader(
      data.Ptr(),
      DWORD(data.Length()),
      nullptr, 
      shaderOut->AddressForReplace()), 
    "create vertex shader");

  CHECK_HRESULT(
    device->CreateInputLayout(
      layoutElements,
      DWORD(layoutElementCount),
      data.Ptr(),
      DWORD(data.Length()),
      layoutOut->AddressForReplace()),
    "create input layout");
}

  
void GraphicsDevice::CreatePixelShader(int resourceID, ComPtr<ID3D11PixelShader> *shaderOut)
{
  auto data = LoadResourceBytes(resourceID);
  CHECK_HRESULT(
    device->CreatePixelShader(
      data.Ptr(),
      DWORD(data.Length()),
      nullptr, 
      shaderOut->AddressForReplace()), 
    "create pixel shader");
}

  
void GraphicsDevice::CreateComputeShader(int resourceID, ComPtr<ID3D11ComputeShader> *shaderOut)
{
  auto data = LoadResourceBytes(resourceID);
  CHECK_HRESULT(
    device->CreateComputeShader(
      data.Ptr(),
      DWORD(data.Length()),
      nullptr, 
      shaderOut->AddressForReplace()), 
    "create compute shader");
}

  
void GraphicsDevice::CreateTexture2D(
  uint32_t width, 
  uint32_t height, 
  uint32_t mipCount,
  DXGI_FORMAT format,
  TextureFlags flags,
  ComPtr<ID3D11Texture2D> *textureOut,
  ComPtr<ID3D11ShaderResourceView> *srvOut,
  ComPtr<ID3D11UnorderedAccessView> *uavOut,
  void *initialDataTexels,
  uint32_t initialDataPitch)
{
  CreateTexture2D(width, height, mipCount, format, flags, textureOut, srvOut, uavOut, nullptr, initialDataTexels, initialDataPitch);
}


void GraphicsDevice::CreateTexture2D(
  uint32_t width, 
  uint32_t height, 
  uint32_t mipCount,
  DXGI_FORMAT format,
  TextureFlags flags,
  ComPtr<ID3D11Texture2D> *textureOut,
  ComPtr<ID3D11ShaderResourceView> *srvOut,
  ComPtr<ID3D11UnorderedAccessView> *uavOut,
  ComPtr<ID3D11RenderTargetView> *rtvOut,
  void *initialDataTexels,
  uint32_t initialDataPitch)
{
  D3D11_TEXTURE2D_DESC desc;
  ZeroType(&desc);
  desc.Width = width;
  desc.Height = height;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.Usage = ((flags & TextureFlags::Dynamic) != TextureFlags::None) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = ((flags & TextureFlags::Dynamic) != TextureFlags::None) ? D3D11_CPU_ACCESS_WRITE : 0;
  desc.MipLevels = mipCount;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  if (((flags & TextureFlags::UAV) != TextureFlags::None))
  {
    desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
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


  CHECK_HRESULT(device->CreateTexture2D(&desc, initialData, textureOut->AddressForReplace()), "create texture 2D");
  CHECK_HRESULT(device->CreateShaderResourceView(textureOut->Ptr(), nullptr, srvOut->AddressForReplace()), "create SRV");
  if (uavOut != nullptr)
  {
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    ZeroType(&uavDesc);
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    CHECK_HRESULT(device->CreateUnorderedAccessView(textureOut->Ptr(), &uavDesc, uavOut->AddressForReplace()), "create UAV");
  }

  if (rtvOut != nullptr)
  {
    CHECK_HRESULT(device->CreateRenderTargetView(textureOut->Ptr(), nullptr, rtvOut->AddressForReplace()), "create RTV");
  }
}


void GraphicsDevice::CreateTexture2D(
  uint32_t width, 
  uint32_t height, 
  DXGI_FORMAT format,
  TextureFlags flags,
  ComPtr<ID3D11Texture2D> *textureOut,
  ComPtr<ID3D11ShaderResourceView> *srvOut,
  ComPtr<ID3D11UnorderedAccessView> *uavOut,
  void *initialDataTexels,
  uint32_t initialDataPitch)
{
  CreateTexture2D(width, height, 1, format, flags, textureOut, srvOut, uavOut, nullptr, initialDataTexels, initialDataPitch);
}


void GraphicsDevice::CreateTexture2D(
  uint32_t width, 
  uint32_t height, 
  DXGI_FORMAT format,
  TextureFlags flags,
  ComPtr<ID3D11Texture2D> *textureOut,
  ComPtr<ID3D11ShaderResourceView> *srvOut,
  ComPtr<ID3D11UnorderedAccessView> *uavOut,
  ComPtr<ID3D11RenderTargetView> *rtvOut,
  void *initialDataTexels,
  uint32_t initialDataPitch)
{
  CreateTexture2D(width, height, 1, format, flags, textureOut, srvOut, uavOut, rtvOut, initialDataTexels, initialDataPitch);
}



void GraphicsDevice::CreateConstantBuffer(size_t size, ComPtr<ID3D11Buffer> *bufferOut)
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
  CHECK_HRESULT(device->CreateBuffer(&desc, nullptr, bufferOut->AddressForReplace()), "create constant buffer");
}


void GraphicsDevice::DiscardAndUpdateBuffer(ID3D11Buffer *buffer, const void *data, size_t dataSize)
{
  D3D11_MAPPED_SUBRESOURCE map;
  context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
  memcpy(map.pData, data, size_t(dataSize));
  context->Unmap(buffer, 0);
}


GraphicsDevice::GraphicsDevice(HWND hwnd)
{
  window = hwnd;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  ZeroType(&swapDesc);
  swapDesc.BufferCount = 2;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.OutputWindow = hwnd;
  swapDesc.Windowed = true;
  swapDesc.SampleDesc.Count = 1;

  // $TODO Handle different refresh rates?
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
}


void GraphicsDevice::UpdateWindowSize()
{
  RECT clientRect;
  GetClientRect(window, &clientRect);

  backbuffer = nullptr;
  backbufferView = nullptr;

  swapChain->ResizeBuffers(
    2, 
    clientRect.right - clientRect.left, 
    clientRect.bottom - clientRect.top, 
    DXGI_FORMAT_R8G8B8A8_UNORM, 
    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);

  swapChain->GetBuffer(0, UUID_AND_ADDRESS(backbuffer));
  CHECK_HRESULT(device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.AddressForReplace()), "create render target view");
}


void GraphicsDevice::Present()
{
  swapChain->Present(1, 0);
}
