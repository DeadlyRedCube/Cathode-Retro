#pragma once

#include <cassert>
#include <cinttypes>

#include "NTSCify/GraphicsDevice.h"
#include "NTSCify/OverscanSettings.h"
#include "NTSCify/ScanlineType.h"
#include "NTSCify/ScreenSettings.h"


namespace NTSCify::Internal::CRT
{
  // This class takes RGB data (either the input or SVideo/composite filtering final output) and draws it as if it were on a CRT screen
  class RGBToCRT
  {
  public:
    RGBToCRT(
      IGraphicsDevice *deviceIn,
      uint32_t inputImageWidthIn,
      uint32_t signalTextureWidthIn,
      uint32_t scanlineCountIn,
      float pixelAspectIn,
      const OverscanSettings &overscan,
      const ScreenSettings &screen)
    : device(deviceIn)
    , inputImageWidth(inputImageWidthIn)
    , signalTextureWidth(signalTextureWidthIn)
    , scanlineCount(scanlineCountIn)
    , pixelAspect(pixelAspectIn)
    , screenSettings(screen)
    , overscanSettings(overscan)
    {
      rgbToScreenShader = device->CreateShader(ShaderID::RGBToCRT);
      generateScreenTextureShader = device->CreateShader(ShaderID::GenerateScreenTexture);
      downsample2XShader = device->CreateShader(ShaderID::Downsample2X);
      gaussianBlurShader = device->CreateShader(ShaderID::GaussianBlur13);
      toneMapShader = device->CreateShader(ShaderID::TonemapAndDownsample);

      screenTextureConstantBuffer = device->CreateConstantBuffer(sizeof(ScreenTextureConstants));
      rgbToScreenConstantBuffer = device->CreateConstantBuffer(sizeof(RGBToScreenConstants));
      samplePatternConstantBuffer = device->CreateConstantBuffer(sizeof(k_samplingPattern16X));
      toneMapConstantBuffer = device->CreateConstantBuffer(sizeof(ToneMapConstants));
      gaussianBlurConstantBufferH = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
      gaussianBlurConstantBufferV = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));

      GenerateShadowMaskTexture();
      UpdateBlurTextures();
    }


    void SetSettings(const OverscanSettings &overscan, const ScreenSettings &screen)
    {
      if (screen != screenSettings || overscan != overscanSettings)
      {
        overscanSettings = overscan;
        screenSettings = screen;

        UpdateBlurTextures();
        if (screenTexture != nullptr)
        {
          RenderScreenTexture();
        }
      }
    }


    void SetOutputSize(uint32_t outputWidth, uint32_t outputHeight)
    {
      if (screenTexture == nullptr
        || screenTexture->Width() != outputWidth
        || screenTexture->Height() != outputHeight)
      {
        // Rebuild the texture at the correct resolution
        screenTexture = device->CreateTexture(outputWidth, outputHeight, 1, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);
        RenderScreenTexture();
      }
    }


    void Render(const ITexture *currentFrameRGBInput, const ITexture *previousFrameRGBInput, ITexture *outputTexture, ScanlineType scanType)
    {
      assert(screenTexture != nullptr);

      device->UpdateConstantBuffer(
        rgbToScreenConstantBuffer.get(),
        RGBToScreenConstants{
          .common = CalculateCommonConstants(CalculateAspectData()),

            // $TODO: may want to artificially increase phosphorDecay if we're interlaced
          .phosphorDecay = screenSettings.phosphorDecay,
          .scanlineCount = float(scanlineCount),
          .scanlineStrength = screenSettings.scanlineStrength,
          .curEvenOddTexelOffset = (scanType != ScanlineType::Even) ? 0.5f : -0.5f,
          .prevEvenOddTexelOffset = (prevScanlineType != ScanlineType::Even) ? 0.5f : -0.5f,
          .diffusionStrength = screenSettings.diffusionStrength,
        });

      if (screenSettings.diffusionStrength > 0.0f)
      {
        RenderBlur(currentFrameRGBInput);
      }

      device->RenderQuad(
        rgbToScreenShader.get(),
        outputTexture,
        {currentFrameRGBInput, previousFrameRGBInput, screenTexture.get(), blurTexture.get()},
        {SamplerType::LinearClamp},
        {rgbToScreenConstantBuffer.get()});

      prevScanlineType = scanType;
    }

  protected:
    // These are float2 sampling patterns, but they need to be float4 aligned for the constant buffers, so there's 2 padding floats per
    //  coordinate.
    // $TODO: Is that actually true in non-D3D graphics APIs? I guess I could make them float4s so that it IS true.
    // These are standard 8x and 16x sampling patterns (found in the D3D docs here:
    //  https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels )
    static constexpr float k_samplingPattern8X[] =
    {
       1.0f / 8.0, -3.0f / 8.0,   0.0f, 0.0f,
      -1.0f / 8.0,  3.0f / 8.0,   0.0f, 0.0f,
       5.0f / 8.0,  1.0f / 8.0,   0.0f, 0.0f,
      -3.0f / 8.0, -5.0f / 8.0,   0.0f, 0.0f,
      -5.0f / 8.0,  5.0f / 8.0,   0.0f, 0.0f,
      -7.0f / 8.0, -1.0f / 8.0,   0.0f, 0.0f,
       3.0f / 8.0,  7.0f / 8.0,   0.0f, 0.0f,
       7.0f / 8.0, -7.0f / 8.0,   0.0f, 0.0f,
    };


    static constexpr float k_samplingPattern16X[] =
    {
       1.0f / 8.0,  1.0f / 8.0,   0.0f, 0.0f,
      -1.0f / 8.0, -3.0f / 8.0,   0.0f, 0.0f,
      -3.0f / 8.0,  2.0f / 8.0,   0.0f, 0.0f,
       4.0f / 8.0, -1.0f / 8.0,   0.0f, 0.0f,
      -5.0f / 8.0, -2.0f / 8.0,   0.0f, 0.0f,
       2.0f / 8.0,  5.0f / 8.0,   0.0f, 0.0f,
       5.0f / 8.0,  3.0f / 8.0,   0.0f, 0.0f,
       3.0f / 8.0, -5.0f / 8.0,   0.0f, 0.0f,
      -2.0f / 8.0,  6.0f / 8.0,   0.0f, 0.0f,
       0.0f / 8.0, -7.0f / 8.0,   0.0f, 0.0f,
      -4.0f / 8.0, -6.0f / 8.0,   0.0f, 0.0f,
      -6.0f / 8.0,  4.0f / 8.0,   0.0f, 0.0f,
      -8.0f / 8.0,  0.0f / 8.0,   0.0f, 0.0f,
       7.0f / 8.0, -4.0f / 8.0,   0.0f, 0.0f,
       6.0f / 8.0,  7.0f / 8.0,   0.0f, 0.0f,
      -7.0f / 8.0, -8.0f / 8.0,   0.0f, 0.0f,
    };


    struct CommonConstants
    {
      float viewScaleX;             // Scale to get the correct aspect ratio of the image
      float viewScaleY;             // ...
      float overscanScaleX;         // overscanSize / standardSize;
      float overscanScaleY;         // ...
      float overscanOffsetX;        // How much texture space to offset the center of our coordinate system due to overscan
      float overscanOffsetY;        // ...
      float distortionX;            // How much to distort (from 0 .. 1)
      float distortionY;            // ...
    };


    struct AspectData
    {
      float overscanSizeX;
      float overscanSizeY;
      float aspect;
    };


    struct ScreenTextureConstants
    {
      CommonConstants common;
      float maskDistortionX;        // Where to put the mask sides
      float maskDistortionY;        // ...

      float shadowMaskScaleX;       // Scale of the shadow mask texture lookup
      float shadowMaskScaleY;       // Scale of the shadow mask texture lookup
      float shadowMaskStrength;     //
      float roundedCornerSize;      // 0 == no corner, 1 == screen is an oval
    };


    struct RGBToScreenConstants
    {
      CommonConstants common;
      float phosphorDecay;          // $TODO: Should really be named phosphorPersistence or something
      float scanlineCount;          // How many scanlines there are
      float scanlineStrength;       // How strong the scanlines are (0 == none, 1 == whoa)
      float curEvenOddTexelOffset;  // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
      float prevEvenOddTexelOffset; // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
      float diffusionStrength;      // The strength of the diffusion blur that is blended into the signal.
    };


    struct GaussianBlurConstants
    {
      float blurDirX;
      float blurDirY;
    };


    struct ToneMapConstants
    {
      float downsampleDirX;
      float downsampleDirY;
      float minLuminosity;
      float colorPower;
    };


    AspectData CalculateAspectData()
    {
      // Figure out how to adjust our viewed texture area for overscan
      float overscanSizeX = float(inputImageWidth - (overscanSettings.overscanLeft + overscanSettings.overscanRight));
      float overscanSizeY = float(scanlineCount - (overscanSettings.overscanTop + overscanSettings.overscanBottom));
      return {
        .overscanSizeX = overscanSizeX,
        .overscanSizeY = overscanSizeY,
        .aspect = pixelAspect * overscanSizeX / overscanSizeY,
      };
    }


    CommonConstants CalculateCommonConstants(const AspectData &aspectData)
    {
      CommonConstants data;

      data.overscanScaleX = aspectData.overscanSizeX / inputImageWidth;
      data.overscanScaleY = aspectData.overscanSizeY / scanlineCount;

      data.overscanOffsetX = float(int32_t(overscanSettings.overscanLeft - overscanSettings.overscanRight)) / inputImageWidth * 0.5f;
      data.overscanOffsetY = float(int32_t(overscanSettings.overscanTop - overscanSettings.overscanBottom)) / scanlineCount * 0.5f;

      // Figure out the aspect ratio of the output, given both our dimensions as well as the pixel aspect ratio in the screen settings.
      if (float(screenTexture->Width()) > aspectData.aspect * float(screenTexture->Height()))
      {
        float desiredWidth = aspectData.aspect * float(screenTexture->Height());
        data.viewScaleX = float(screenTexture->Width()) / desiredWidth;
        data.viewScaleY = 1.0f;
      }
      else
      {
        float desiredHeight = float(screenTexture->Width()) / aspectData.aspect;
        data.viewScaleX = 1.0f;
        data.viewScaleY = float(screenTexture->Height()) / desiredHeight;
      }

      data.distortionX = screenSettings.horizontalDistortion;
      data.distortionY = screenSettings.verticalDistortion;

      return data;
    }


    void RenderScreenTexture()
    {
      assert(screenTexture != nullptr);

      ScreenTextureConstants data;

      auto aspectData = CalculateAspectData();
      data.common = CalculateCommonConstants(aspectData);
      data.maskDistortionX = screenSettings.screenEdgeRoundingX + data.common.distortionX;
      data.maskDistortionY = screenSettings.screenEdgeRoundingY + data.common.distortionY;
      data.roundedCornerSize = screenSettings.cornerRounding;

      // The values for shadowMaskScale were initially normalized against a 240-pixel-tall screen so just pretend it's ALWAYS that height
      //  for purposes of scaling the shadow mask.
      static constexpr float shadowMaskScaleNormalization = 240.0f * 0.7f;

      data.shadowMaskScaleX = float (inputImageWidth) / float(scanlineCount)
                            * pixelAspect
                            * shadowMaskScaleNormalization
                            * 0.45f
                            / screenSettings.shadowMaskScale;
      data.shadowMaskScaleY = shadowMaskScaleNormalization / screenSettings.shadowMaskScale;
      data.shadowMaskStrength = screenSettings.shadowMaskStrength;

      device->BeginRendering();

      device->UpdateConstantBuffer(screenTextureConstantBuffer.get(), data);
      device->UpdateConstantBuffer(samplePatternConstantBuffer.get(), k_samplingPattern16X);

      device->RenderQuad(
        generateScreenTextureShader.get(),
        screenTexture.get(),
        {shadowMaskTexture.get()},
        {SamplerType::LinearWrap},
        {screenTextureConstantBuffer.get(), samplePatternConstantBuffer.get()});
      device->EndRendering();
    }


    void UpdateBlurTextures()
    {
      auto aspectData = CalculateAspectData();
      uint32_t tonemapTexWidth;
      uint32_t tonemapTexHeight;
      uint32_t blurTextureWidth;
      if (float(signalTextureWidth) > aspectData.aspect * float(scanlineCount))
      {
        // the tonemap texture is going to scale down to 2x the actual aspect we want (we'll downsample farther from there)
        tonemapTexHeight = scanlineCount;
        tonemapTexWidth = uint32_t(std::round(aspectData.aspect * float(scanlineCount))) * 2;
        blurTextureWidth = tonemapTexWidth / 2;
        downsampleDirX = 1.0f;
        downsampleDirY = 0.0f;
      }
      else
      {
        // This is an unlikely case (the signal already has a massive stretch), in this case we'll keep things the same and the
        //  blur texture will be scaled up slightly.
        tonemapTexWidth = inputImageWidth;
        tonemapTexHeight = scanlineCount;
        blurTextureWidth = uint32_t(std::round(aspectData.aspect * float(scanlineCount)));
        downsampleDirX = 0.0f;
        downsampleDirY = 1.0f;
      }

      if (toneMapTexture == nullptr || toneMapTexture->Width() != tonemapTexWidth || toneMapTexture->Height() != tonemapTexHeight)
      {
        // Rebuild our blur textures.
        toneMapTexture = device->CreateTexture(
          tonemapTexWidth,
          tonemapTexHeight,
          1,
          TextureFormat::RGBA_Unorm8,
          TextureFlags::RenderTarget);

        blurTexture = device->CreateTexture(
          blurTextureWidth,
          tonemapTexHeight,
          1,
          TextureFormat::RGBA_Unorm8,
          TextureFlags::RenderTarget);

        blurScratchTexture = device->CreateTexture(
          blurTextureWidth,
          tonemapTexHeight,
          1,
          TextureFormat::RGBA_Unorm8,
          TextureFlags::RenderTarget);
      }
    }


    // Generate the shadow mask texture we use for the CRT emulation
    void GenerateShadowMaskTexture()
    {
      // $TODO this texture could be pre-made and the one being generated here is WAY overkill for how tiny it shows up on-screen, but it does look nice!
      static constexpr uint32_t k_size = 512;
      static constexpr uint32_t k_mipCount = 8;

      shadowMaskTexture = device->CreateTexture(k_size, k_size / 2, k_mipCount, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);

      // First step is the generate the texture at the largest mip level
      auto generateShadowMaskShader = device->CreateShader(ShaderID::GenerateShadowMask);

      struct GenerateShadowMaskConstants
      {
        float blackLevel;
        float coordinateScale;
        float texWidth;
        float texHeight;
      };

      auto constBuf = device->CreateConstantBuffer(sizeof(GenerateShadowMaskConstants));

      device->BeginRendering();
      {
        device->UpdateConstantBuffer(
          constBuf.get(),
          GenerateShadowMaskConstants {
            .blackLevel = 0.0,
            .coordinateScale = 1.0f / float(k_size),
            .texWidth = float(k_size),
            .texHeight = float(k_size / 2),
          });

        device->RenderQuad(
          generateShadowMaskShader.get(),
          shadowMaskTexture.get(),
          {},
          {SamplerType::LinearClamp},
          {constBuf.get()});

        // Now it's generated so we need to generate the mips by using our lanczos downsample
        for (uint32_t destMip = 1; destMip < k_mipCount; destMip++)
        {
          device->RenderQuad(
            downsample2XShader.get(),
            {shadowMaskTexture.get(), destMip},
            {{shadowMaskTexture.get(), destMip - 1}},
            {SamplerType::LinearWrap},
            {});
        }
      }
      device->EndRendering();
    }


    void RenderBlur(const ITexture *inputTexture)
    {
      // Step one: abuse the downsample2x shader to scale to whatever size we need.
      // $TODO: This is slightly inaccurate, we should really be using the max of inputTexture and prevFrameTexture * g_phosphorDecay but
      //  for now, this is fine.
      device->UpdateConstantBuffer(
        toneMapConstantBuffer.get(),
        ToneMapConstants {
          .downsampleDirX = downsampleDirX,
          .downsampleDirY = downsampleDirY,

          // $TODO: Probably want to expose these values too, since everything else is an option
          .minLuminosity = 0.0f,
          .colorPower = 1.3f,
        });
      device->RenderQuad(
        toneMapShader.get(),
        toneMapTexture.get(),
        {inputTexture},
        {SamplerType::LinearClamp},
        {toneMapConstantBuffer.get()});

      device->RenderQuad(
        downsample2XShader.get(),
        blurTexture.get(),
        {toneMapTexture.get()},
        {SamplerType::LinearClamp},
        {});

      device->UpdateConstantBuffer(gaussianBlurConstantBufferH.get(), GaussianBlurConstants{1.0f, 0.0f});
      device->RenderQuad(
        gaussianBlurShader.get(),
        blurScratchTexture.get(),
        {blurTexture.get()},
        {SamplerType::LinearClamp},
        {gaussianBlurConstantBufferH.get()});

      device->UpdateConstantBuffer(gaussianBlurConstantBufferV.get(), GaussianBlurConstants{0.0f, 1.0f});
      device->RenderQuad(
        gaussianBlurShader.get(),
        blurTexture.get(),
        {blurScratchTexture.get()},
        {SamplerType::LinearClamp},
        {gaussianBlurConstantBufferV.get()});
    }


    IGraphicsDevice *device;

    uint32_t inputImageWidth;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
    float pixelAspect;

    std::unique_ptr<IConstantBuffer> screenTextureConstantBuffer;
    std::unique_ptr<IConstantBuffer> rgbToScreenConstantBuffer;
    std::unique_ptr<IConstantBuffer> samplePatternConstantBuffer;
    std::unique_ptr<IConstantBuffer> toneMapConstantBuffer;
    std::unique_ptr<IConstantBuffer> gaussianBlurConstantBufferH;
    std::unique_ptr<IConstantBuffer> gaussianBlurConstantBufferV;
    std::unique_ptr<IShader> rgbToScreenShader;
    std::unique_ptr<IShader> downsample2XShader;
    std::unique_ptr<IShader> toneMapShader;
    std::unique_ptr<IShader> gaussianBlurShader;
    std::unique_ptr<IShader> generateScreenTextureShader;

    std::unique_ptr<ITexture> shadowMaskTexture;
    std::unique_ptr<ITexture> screenTexture;

    std::unique_ptr<ITexture> toneMapTexture;
    std::unique_ptr<ITexture> blurScratchTexture;
    std::unique_ptr<ITexture> blurTexture;

    ScreenSettings screenSettings;
    OverscanSettings overscanSettings;
    bool screenSettingsDirty = false;
    ScanlineType prevScanlineType = ScanlineType::Progressive;
    float downsampleDirX;
    float downsampleDirY;
  };
}