#pragma once

#include "Util.h"

enum class TextureFlags
{
  Dynamic       = 0x01,
  RenderTarget  = 0x02,

  None          = 0x00,
  All           = 0x03,
};


class IConstantBuffer
{
public:
  virtual ~IConstantBuffer() = default;
};


class IShader
{
public:
  virtual ~IShader() = default;
};


enum class ShaderID
{
  Downsample2X,

  GeneratePhaseTexture,
  RGBToSVideoOrComposite,
  ApplyArtifacts,

  CompositeToSVideo,
  SVideoToYIQ,
  YIQToRGB,
  FilterRGB,

  GenerateScreenTexture,
  GenerateShadowMask,
  TonemapAndDownsample,
  GaussianBlur13,
  RGBToCRT,
};


enum class TextureFormat
{
  RGBA_Unorm8,
  R_Float32,
  RG_Float32,
  RGBA_Float32,
};


class ITexture
{
public:
  virtual ~ITexture() = default;
  virtual uint32_t Width() const = 0;
  virtual uint32_t Height() const = 0;
  virtual uint32_t MipCount() const = 0;
  virtual TextureFormat Format() const = 0;

protected:
  ITexture() = default;
};


enum class SamplerType
{
  LinearClamp,
  LinearWrap,
};


struct ShaderResourceView
{
  ShaderResourceView(const ITexture *tex)
    : texture(tex)
    { }

  ShaderResourceView(const ITexture *tex, uint32_t mip)
    : texture(tex)
    , mipLevel(int32_t(mip))
    { }
    
  const ITexture *texture;
  int32_t mipLevel = -1;
};


struct RenderTargetView
{
  RenderTargetView(ITexture *tex)
    : texture(tex)
    { }

  RenderTargetView(ITexture *tex, uint32_t mip)
    : texture(tex)
    , mipLevel(int32_t(mip))
    { }
    
  ITexture *texture;
  uint32_t mipLevel = 0;
};


class IGraphicsDevice
{
public:
  virtual ~IGraphicsDevice() = default;

  virtual uint32_t OutputWidth() const = 0;
  virtual uint32_t OutputHeight() const = 0;

  virtual std::unique_ptr<ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount, // 0 means "all mip levels"
    TextureFormat format,
    TextureFlags flags,
    void *initialDataTexels = nullptr,
    uint32_t initialDataPitch = 0) = 0;

  virtual std::unique_ptr<IConstantBuffer> CreateConstantBuffer(size_t size) = 0;

  virtual std::unique_ptr<IShader> CreateShader(ShaderID id) = 0;

  virtual void DiscardAndUpdateBuffer(IConstantBuffer *buffer, const void *data, size_t dataSize) = 0;

  template <typename T>
  void DiscardAndUpdateBuffer(IConstantBuffer *buffer, const T *data)
  {
    DiscardAndUpdateBuffer(buffer, data, sizeof(T));
  }

  virtual void RenderQuad(
    IShader *ps,
    RenderTargetView output,
    std::initializer_list<ShaderResourceView> inputs,
    std::initializer_list<SamplerType> samplers,
    std::initializer_list<IConstantBuffer *> constantBuffers) = 0;
};


