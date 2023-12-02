// This file contains graphics interfaces that an including project should implement to pass to Cathode Retro for
//  rendering - see the example projects for different API examples.
// Note that most of these things use D3D terminology, since that's my standard reference frame.
#pragma once

#include <memory>

namespace CathodeRetro
{
  // This is a "constant buffer" (GL/Vulkan refer to these as "uniform buffers" - basically a data buffer to be handed
  //  to a shader. These will be fully updated every frame so it's valid for this to allocate GPU bytes out of a pool
  //  and update for graphics APIs that prefer that style of CPU -> GPU buffering. These may be updated more than once
  //  per frame.
  class IConstantBuffer
  {
  public:
    virtual ~IConstantBuffer() = default;

    // Copy the given data bytes into the constant buffer so that it is ready for rendering.
    virtual void Update(const void *data, size_t dataByteCount) = 0;

    // Templated version of Update that takes a type and updates the whole type.
    template <typename T>
    void Update(const T &data)
    {
      static_assert(!std::is_pointer<T>::value, "Cannot call templated Update with a pointer type.");
      Update(&data, sizeof(T));
    }
  };


  // Cathode Retro has shaders with these identifiers, it can ask for shaders with these IDs, and it is up to the
  //  graphics device to define how these IDs get translated into the actual loaded shader.
  enum class ShaderID
  {
    Util_Copy,                                      // cathode-retro-util-copy.hlsl
    Util_Downsample2X,                              // cathode-retro-util-downsample-2x.hlsl
    Util_TonemapAndDownsample,                      // cathode-retro-util-tonemap-and-downsample.hlsl
    Util_GaussianBlur13,                            // cathode-retro-util-gaussian-blur.hlsl

    Generator_GeneratePhaseTexture,                 // cathode-retro-generator-gen-phase.hlsl
    Generator_RGBToSVideoOrComposite,               // cathode-retro-generator-rgb-to-svideo-or-compsite.hlsl
    Generator_ApplyArtifacts,                       // cathode-retro-generator-apply-artifacts.hlsl

    Decoder_CompositeToSVideo,                      // cathode-retro-decoder-composite-to-svideo.hlsl
    Decoder_SVideoToModulatedChroma,                // cathode-retro-decoder-svideo-to-modulated-chroma.hlsl
    Decoder_SVideoToRGB,                            // cathode-retro-decoder-svideo-to-rgb.hlsl
    Decoder_FilterRGB,                              // cathode-retro-decoder-filter-rgb.hlsl

    CRT_GenerateScreenTexture,                      // cathode-retro-crt-generate-screen-texture.hlsl
    CRT_GenerateSlotMask,                           // cathode-retro-crt-generate-slot-mask.hlsl
    CRT_GenerateShadowMask,                         // cathode-retro-crt-generate-shadow-mask.hlsl
    CRT_GenerateApertureGrille,                     // cathode-retro-crt-generate-aperture-grille.hlsl
    CRT_RGBToCRT,                                   // cathode-retro-crt-rgb-to-crt.hlsl
  };


  // Cathode Retro uses standard RGBA_Unorm8 textures (the component ordering doesn't matter so if an API/platform
  //  needs it to be BGRA or the like, that is totally fine), as well as 1- 2- and 4-component float textures (for the
  //  generated signal data)
  enum class TextureFormat
  {
    RGBA_Unorm8,
    R_Float32,
    RG_Float32,
    RGBA_Float32,
  };


  // This interface represents a wrapper around a texture, as you might have guessed. It exposes a few metrics for
  //  Cathode Retro to query.
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


  // This interface represents a wrapper around a render target, and is derived from ITexture.
  class IRenderTarget : public ITexture
  {
  protected:
    IRenderTarget() = default;
  };


  // The type of texture sampling and addressing to use for a given sampler.
  enum class SamplerType
  {
    NearestClamp,
    NearestWrap,
    LinearClamp,
    LinearWrap,
  };


  // This represents a view to the input to a shader - it has a texture, a sampler type, and an optional mipmap level.
  //  If the mip level is specified, the sampling of the texture should be restricted to only that level, otherwise all
  //  mip levels should be samplable (using linear or, more ideally, anisotropic mip level filtering)
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


  // This represents a view output of a shader. It has a texture and an optional target mipmap level. If no mipmap
  //  level is specified, it will render to the largest mip level.
  struct RenderTargetView
  {
    RenderTargetView(IRenderTarget *tex, uint32_t mip = 0)
      : texture(tex)
      , mipLevel(int32_t(mip))
      { }

    IRenderTarget *texture;
    uint32_t mipLevel = 0;
  };


  // This is the main interface that Cathode Retro uses to interact with the graphics device. It can create objects
  //  (render targets, constant buffers, shaders) and render.
  class IGraphicsDevice
  {
  public:
    virtual ~IGraphicsDevice() = default;

    // This should create a render target with the given dimensions, mip count, and format.
    virtual std::unique_ptr<IRenderTarget> CreateRenderTarget(
      uint32_t width,
      uint32_t height,
      uint32_t mipCount, // 0 means "all mip levels"
      TextureFormat format) = 0;

    // Create a constant buffer that can hold the given amount of bytes.
    virtual std::unique_ptr<IConstantBuffer> CreateConstantBuffer(size_t byteCount) = 0;

    // This is called when Cathode Retro is beginning its rendering, and is a great place to set up any render state
    //  that is going to be consistent across the whole pipeline (the vertex shader, blending mode, etc).
    // Cathode Retro specifically wants no alpha blending or testing enabled. Additionally, it expects floating-point
    //  textures to be able to use the full range of values, so if the API allows for truncating floating-point values
    //  to the 0..1 range on either shader output or sampling input, that should be disabled.
    virtual void BeginRendering() = 0;

    // Render a quad using the given objects.
    virtual void RenderQuad(
      ShaderID shaderID,
      RenderTargetView output,
      std::initializer_list<ShaderResourceView> inputs,
      IConstantBuffer *constantBuffer = nullptr) = 0;

    // This is called when Cathode Retro is done rendering, and is a good spot for render state to be restored back to
    //  whatever the enclosing app expects (i.e. if it's a game, the game probably has its own standard state setup).
    virtual void EndRendering() = 0;
  };
}

