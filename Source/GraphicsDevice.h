#pragma once
#include <memory>

#define NOMINMAX

#include <d3d11.h>
#include <Windows.h>

#include "ComPtr.h"
#include "SimpleArray.h"
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


enum class TextureFlags
{
  Dynamic       = 0x01,
  RenderTarget  = 0x02,

  None          = 0x00,
  All           = 0x03,
};


class ITexture
{
public:
  virtual ~ITexture() = default;
  virtual uint32_t Width() const = 0;
  virtual uint32_t Height() const = 0;
  virtual uint32_t MipCount() const = 0;
  virtual DXGI_FORMAT Format() const = 0;

protected:
  ITexture() = default;
};


class IMipLevelSource
{
public:
  virtual ~IMipLevelSource() = default;
  virtual uint32_t Width() const = 0;
  virtual uint32_t Height() const = 0;

protected:
  IMipLevelSource() = default;
};


class IMipLevelTarget
{
public:
  virtual ~IMipLevelTarget() = default;
  virtual uint32_t Width() const = 0;
  virtual uint32_t Height() const = 0;

protected:
  IMipLevelTarget() = default;
};


enum class SamplerType
{
  Clamp,
  Wrap,
};

// A rather minimal wrapper around a D3D device and related functionality
class GraphicsDevice
{
public:
  GraphicsDevice(HWND hwnd);
  
  GraphicsDevice(GraphicsDevice &) = delete;
  void operator=(const GraphicsDevice &) = delete;

  void UpdateWindowSize();

  uint32_t BackbufferWidth() const
    { return backbufferWidth; }

  uint32_t BackbufferHeight() const
    { return backbufferHeight; }

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
  
  ComPtr<ID3D11PixelShader> CreatePixelShader(int resourceID);
  ComPtr<ID3D11Buffer> CreateConstantBuffer(size_t size);

  void DiscardAndUpdateBuffer(ID3D11Buffer *buffer, const void *data, size_t dataSize);

  template <typename T>
  void DiscardAndUpdateBuffer(ID3D11Buffer *buffer, const T *data)
  {
    DiscardAndUpdateBuffer(buffer, data, sizeof(T));
  }


  SimpleArray<uint32_t> GetTexturePixels(ITexture *texture);

  std::unique_ptr<ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount,
    DXGI_FORMAT format,
    TextureFlags flags,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0);


  std::unique_ptr<ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    TextureFlags flags,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0)
    { return CreateTexture(width, height, 1, format, flags, initialDataTexels, initialDataPitch); }


  std::unique_ptr<IMipLevelSource> CreateMipLevelSource(ITexture *texture, uint32_t mipLevel);
  std::unique_ptr<IMipLevelTarget> CreateMipLevelTarget(ITexture *texture, uint32_t mipLevel);

  void RenderQuadWithPixelShader(
    ID3D11PixelShader *ps,
    nullptr_t,
    std::initializer_list<const ITexture *> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<ID3D11Buffer *> constantBuffers);

  void RenderQuadWithPixelShader(
    ID3D11PixelShader *ps,
    IMipLevelTarget *output,
    std::initializer_list<const IMipLevelSource *> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<ID3D11Buffer *> constantBuffers);

  void RenderQuadWithPixelShader(
    ID3D11PixelShader *ps,
    ITexture *output,
    std::initializer_list<const ITexture *> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<ID3D11Buffer *> constantBuffers);

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

    DXGI_FORMAT Format() const override
      { return format; }

    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11RenderTargetView> rtv;
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
    DXGI_FORMAT format;
  };


  class MipLevelSource : public IMipLevelSource
  {
  public:
    uint32_t Width() const override
      { return width; }

    uint32_t Height() const override
      { return height; }    

    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;

    uint32_t width;
    uint32_t height;
  };

  class MipLevelTarget : public IMipLevelTarget
  {
  public:
    uint32_t Width() const override
      { return width; }

    uint32_t Height() const override
      { return height; }    

    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> rtv;

    uint32_t width;
    uint32_t height;
  };

  void RenderQuadWithPixelShader(
    ID3D11PixelShader *ps,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    ID3D11RenderTargetView *outputRtv,
    std::initializer_list<const ITexture *> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<ID3D11Buffer *> constantBuffers);
    
  void RenderQuadWithPixelShader(
    ID3D11PixelShader *ps,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    ID3D11RenderTargetView *outputRtv,
    ID3D11ShaderResourceView **srvs,
    uint32_t srvCount,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<ID3D11Buffer *> constantBuffers);
    
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