#pragma once

#include <memory>
#include "NTSCify/FlagEnum.h"

namespace NTSCify
{
  class IConstantBuffer
  {
  public:
    virtual ~IConstantBuffer() = default;

    virtual void Update(const void *data, size_t dataSize) = 0;

    // Templated version of Update that takes a type and updates the whole type.
    template <typename T> requires (!std::is_pointer_v<T>)
    void Update(const T &data)
    {
      Update(&data, sizeof(T));
    }
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
    SVideoToRGB,
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
    ShaderResourceView(const ITexture *tex, SamplerType samp)
      : texture(tex)
      , samplerType(samp)
      { }

    ShaderResourceView(const ITexture *tex, uint32_t mip, SamplerType samp)
      : texture(tex)
      , mipLevel(int32_t(mip))
      , samplerType(samp)
      { }

    const ITexture *texture;
    int32_t mipLevel = -1;
    SamplerType samplerType;
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

    virtual std::unique_ptr<ITexture> CreateRenderTarget(
      uint32_t width,
      uint32_t height,
      uint32_t mipCount, // 0 means "all mip levels"
      TextureFormat format) = 0;

    virtual std::unique_ptr<IConstantBuffer> CreateConstantBuffer(size_t size) = 0;
    virtual std::unique_ptr<IShader> CreateShader(ShaderID id) = 0;

    virtual void BeginRendering() = 0;

    virtual void RenderQuad(
      IShader *ps,
      RenderTargetView output,
      std::initializer_list<ShaderResourceView> inputs,
      std::initializer_list<IConstantBuffer *> constantBuffers) = 0;

    virtual void EndRendering() = 0;
  };
}

