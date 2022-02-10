#pragma once

#define NOMINMAX

#include <d3d11.h>
#include <Windows.h>

#include "ComPtr.h"
#include "SimpleArray.h"

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


// A rather minimal wrapper around a D3D device and related functionality
class GraphicsDevice
{
public:
  GraphicsDevice(HWND hwnd);
  
  GraphicsDevice(GraphicsDevice &) = delete;
  void operator=(const GraphicsDevice &) = delete;

  void UpdateWindowSize();

  ID3D11Texture2D *BackbufferTexture()
    { return backbuffer; }

  ID3D11RenderTargetView *BackbufferView()
    { return backbufferView; }

  void ClearBackbuffer();

  void Present();

  ID3D11Device *D3DDevice()
    { return device; }

  ID3D11DeviceContext *Context()
    { return context; }


  void CreateVertexShaderAndInputLayout(
    int resourceID, 
    D3D11_INPUT_ELEMENT_DESC *layoutElements,
    size_t layoutElementCount,
    ComPtr<ID3D11VertexShader> *shaderOut, 
    ComPtr<ID3D11InputLayout> *layoutOut);
  void CreatePixelShader(int resourceID, ComPtr<ID3D11PixelShader> *shaderOut);
  void CreateComputeShader(int resourceID, ComPtr<ID3D11ComputeShader> *shaderOut);
  void CreateConstantBuffer(size_t size, ComPtr<ID3D11Buffer> *bufferOut);

  void DiscardAndUpdateBuffer(ID3D11Buffer *buffer, const void *data, size_t dataSize);

  template <typename T>
  void DiscardAndUpdateBuffer(ID3D11Buffer *buffer, const T *data)
  {
    DiscardAndUpdateBuffer(buffer, data, sizeof(T));
  }

  enum class TextureFlags
  {
    Dynamic = 0x01,
    UAV = 0x02,

    None = 0x00,
    All = 0x03,
  };


  void CreateTexture2D(
    uint32_t width, 
    uint32_t height, 
    uint32_t mipCount,
    DXGI_FORMAT format,
    TextureFlags flags,
    ComPtr<ID3D11Texture2D> *textureOut,
    ComPtr<ID3D11ShaderResourceView> *srvOut,
    ComPtr<ID3D11UnorderedAccessView> *uavOut = nullptr,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0);

  void CreateTexture2D(
    uint32_t width, 
    uint32_t height, 
    DXGI_FORMAT format,
    TextureFlags flags,
    ComPtr<ID3D11Texture2D> *textureOut,
    ComPtr<ID3D11ShaderResourceView> *srvOut,
    ComPtr<ID3D11UnorderedAccessView> *uavOut = nullptr,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0);

private:
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain> swapChain;
  ComPtr<ID3D11Texture2D> backbuffer;
  ComPtr<ID3D11RenderTargetView> backbufferView;

  HWND window;
};