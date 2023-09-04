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
      toneMapConstantBuffer = device->CreateConstantBuffer(sizeof(ToneMapConstants));
      blurDownsampleConstantBuffer = device->CreateConstantBuffer(sizeof(Vec2));
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

          // $TODO: may want to artificially increase phosphorPersistence if we're interlaced
          .phosphorPersistence = screenSettings.phosphorPersistence,
          .scanlineCount = float(scanlineCount),
          .scanlineStrength = screenSettings.scanlineStrength,
          .curEvenOddTexelOffset = (scanType != ScanlineType::Even) ? 0.5f : -0.5f,
          .prevEvenOddTexelOffset = (prevScanlineType != ScanlineType::Even) ? 0.5f : -0.5f,
          .diffusionStrength = screenSettings.diffusionStrength,
          .shadowMaskStrength = screenSettings.shadowMaskStrength,
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
    struct Vec2
    {
      float x;
      float y;
    };


    struct AspectData
    {
      float overscanSizeX;
      float overscanSizeY;
      float aspect;
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


    struct ScreenTextureConstants
    {
      CommonConstants common;

      float maskDistortionX;        // Where to put the mask sides
      float maskDistortionY;        // ...

      float shadowMaskScaleX;       // Scale of the shadow mask texture lookup
      float shadowMaskScaleY;       // Scale of the shadow mask texture lookup
      float roundedCornerSize;      // 0 == no corner, 1 == screen is an oval
    };


    struct RGBToScreenConstants
    {
      CommonConstants common;
      float phosphorPersistence;
      float scanlineCount;          // How many scanlines there are
      float scanlineStrength;       // How strong the scanlines are (0 == none, 1 == whoa)
      float curEvenOddTexelOffset;  // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
      float prevEvenOddTexelOffset; // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
      float diffusionStrength;      // The strength of the diffusion blur that is blended into the signal.
      float shadowMaskStrength;     // How much to blend the shadow mask in. 0 means "no shadow mask" and 1 means "fully apply"
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

      device->BeginRendering();

      device->UpdateConstantBuffer(screenTextureConstantBuffer.get(), data);

      device->RenderQuad(
        generateScreenTextureShader.get(),
        screenTexture.get(),
        {shadowMaskTexture.get()},
        {SamplerType::LinearWrap},
        {screenTextureConstantBuffer.get()});
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

      shadowMaskTexture = device->CreateTexture(k_size, k_size / 2, 0, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);

      // First step is the generate the texture at the largest mip level
      auto generateShadowMaskShader = device->CreateShader(ShaderID::GenerateShadowMask);

      auto halfWidthTexture = device->CreateTexture(k_size / 2, k_size / 2, 0, TextureFormat::RGBA_Unorm8, TextureFlags::RenderTarget);
      struct GenerateShadowMaskConstants
      {
        float texWidth;
        float texHeight;

        float blackLevel;
      };

      auto constBuf = device->CreateConstantBuffer(sizeof(GenerateShadowMaskConstants));

      auto downsampleHConstBuf = device->CreateConstantBuffer(sizeof(Vec2));
      auto downsampleVConstBuf = device->CreateConstantBuffer(sizeof(Vec2));

      device->BeginRendering();
      {
        device->UpdateConstantBuffer(
          constBuf.get(),
          GenerateShadowMaskConstants {
            .texWidth = float(k_size),
            .texHeight = float(k_size / 2),
            .blackLevel = 0.0,
          });

        device->UpdateConstantBuffer(
          downsampleHConstBuf.get(),
          Vec2{ 1.0f, 0.0f });

        device->UpdateConstantBuffer(
          downsampleVConstBuf.get(),
          Vec2{ 0.0f, 1.0f });

        device->RenderQuad(
          generateShadowMaskShader.get(),
          shadowMaskTexture.get(),
          {},
          {SamplerType::LinearClamp},
          {constBuf.get()});

        // Now it's generated so we need to generate the mips by using our lanczos downsample
        for (uint32_t destMip = 1; destMip < shadowMaskTexture->MipCount(); destMip++)
        {
          device->RenderQuad(
            downsample2XShader.get(),
            {halfWidthTexture.get(), destMip - 1},
            {{shadowMaskTexture.get(), destMip - 1}},
            {SamplerType::LinearWrap},
            {downsampleHConstBuf.get()});

          device->RenderQuad(
            downsample2XShader.get(),
            {shadowMaskTexture.get(), destMip},
            {{halfWidthTexture.get(), destMip - 1}},
            {SamplerType::LinearWrap},
            {downsampleVConstBuf.get()});
        }
      }
      device->EndRendering();
    }


    void RenderBlur(const ITexture *inputTexture)
    {
      // $TODO: This is slightly inaccurate, we should really be using the max of inputTexture and prevFrameTexture * phosphorPersistence
      //  but for now, this is fine.
      device->UpdateConstantBuffer(
        toneMapConstantBuffer.get(),
        ToneMapConstants {
          .downsampleDirX = downsampleDirX,
          .downsampleDirY = downsampleDirY,

          // $TODO: Probably want to expose these values too, since everything else is an option
          .minLuminosity = 0.0f,
          .colorPower = 1.3f,
        });

      // We're downsampling "2x" horizontally (scare quotes because it isn't always exactly 2x but it's close enough that we can just
      //  abuse this shader as if it were)
      device->UpdateConstantBuffer(
        blurDownsampleConstantBuffer.get(),
        Vec2{ 1.0f, 0.0f });

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
        {blurDownsampleConstantBuffer.get()});

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
    std::unique_ptr<IConstantBuffer> toneMapConstantBuffer;
    std::unique_ptr<IConstantBuffer> blurDownsampleConstantBuffer;
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