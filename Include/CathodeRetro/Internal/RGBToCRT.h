#pragma once

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <utility>

#include "CathodeRetro/GraphicsDevice.h"
#include "CathodeRetro/Settings.h"


namespace CathodeRetro
{
  namespace Internal
  {
    // This class takes RGB data (either the input or SVideo/composite filtering final output) and draws it as if it
    //  were on a CRT screen
    class RGBToCRT
    {
    public:
      RGBToCRT(
        IGraphicsDevice *deviceIn,
        uint32_t originalInputImageWidthIn,
        uint32_t processedRGBTextureWidthIn,
        uint32_t scanlineCountIn,
        float pixelAspectIn)
      : device(deviceIn)
      , originalInputImageWidth(originalInputImageWidthIn)
      , processedRGBTextureWidth(processedRGBTextureWidthIn)
      , scanlineCount(scanlineCountIn)
      , pixelAspect(pixelAspectIn)
      {
        screenTextureConstantBuffer = device->CreateConstantBuffer(sizeof(ScreenTextureConstants));
        rgbToScreenConstantBuffer = device->CreateConstantBuffer(sizeof(RGBToScreenConstants));
        toneMapConstantBuffer = device->CreateConstantBuffer(sizeof(ToneMapConstants));
        blurDownsampleConstantBuffer = device->CreateConstantBuffer(sizeof(Vec2));
        gaussianBlurConstantBufferH = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
        gaussianBlurConstantBufferV = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
        generateMaskConstantBuffer = device->CreateConstantBuffer(sizeof(Vec2));
        maskDownsampleConstantBufferH = device->CreateConstantBuffer(sizeof(Vec2));
        maskDownsampleConstantBufferV = device->CreateConstantBuffer(sizeof(Vec2));

        prevRGBInput = device->CreateRenderTarget(
          processedRGBTextureWidth,
          scanlineCount,
          1,
          TextureFormat::RGBA_Unorm8);
        maskTexture = device->CreateRenderTarget(k_maskSize, k_maskSize / 2, 0, TextureFormat::RGBA_Unorm8);
        halfWidthMaskTexture = device->CreateRenderTarget(
          k_maskSize / 2,
          k_maskSize / 2,
          0,
          TextureFormat::RGBA_Unorm8);

        needsRenderMaskTexture = true;
        UpdateBlurTextures();
      }


      void SetSettings(const OverscanSettings &overscan, const ScreenSettings &screen)
      {
        if (screen != screenSettings || overscan != overscanSettings)
        {
          if (screen.maskType != screenSettings.maskType)
          {
            needsRenderMaskTexture = true;
          }

          overscanSettings = overscan;
          screenSettings = screen;

          UpdateBlurTextures();
          needsRenderScreenTexture = true;
        }
      }


      void SetOutputSize(uint32_t outputWidth, uint32_t outputHeight)
      {
        if (screenTexture == nullptr
          || screenTexture->Width() != outputWidth
          || screenTexture->Height() != outputHeight)
        {
          // Rebuild the texture at the correct resolution
          screenTexture = device->CreateRenderTarget(outputWidth, outputHeight, 1, TextureFormat::RGBA_Unorm8);
          needsRenderScreenTexture = true;
        }
      }


      void Render(
        const ITexture *currentFrameRGBInput,
        IRenderTarget *outputTexture,
        ScanlineType scanType)
      {
        assert(screenTexture != nullptr);

        if (needsRenderMaskTexture)
        {
          RenderMaskTexture();
          needsRenderMaskTexture = false;
        }

        if (needsRenderScreenTexture)
        {
          RenderScreenTexture();
          needsRenderScreenTexture = false;
        }

        if (isFirstFrame)
        {
          isFirstFrame = false;
          device->RenderQuad(
            ShaderID::Util_Copy,
            prevRGBInput.get(),
            { { currentFrameRGBInput, SamplerType::LinearClamp } });
        }

        // Between 4k and 2k (2160p and 1080p vertical resolution) we want to scale up the effect of the scanlines
        //  and mask (up to a maximum of 1.0, which means that some higher values don't have any effect at 1080p). The
        //  resulting scale values here were eyeballed to make the 2k version look reasonably consistent with the 4k
        //  one.
        float resolutionEffectScale = std::max(
          0.0f,
          std::min(1.0f, 1.0f - (float(screenTexture->Height()) - 1080.0f) / 1080.0f));

        rgbToScreenConstantBuffer->Update(
          RGBToScreenConstants{
            CalculateCommonConstants(CalculateAspectData()),
            screenSettings.borderColor,

            // $TODO: may want to artificially increase phosphorPersistence if we're interlaced
            screenSettings.phosphorPersistence,
            float(scanlineCount),
            std::min(1.0f, screenSettings.scanlineStrength * (resolutionEffectScale + 1.0f)),
            (scanType != ScanlineType::Even) ? 0.5f : -0.5f,
            (prevScanlineType != ScanlineType::Even) ? 0.5f : -0.5f,
            screenSettings.diffusionStrength,
            std::min(1.0f, screenSettings.maskStrength * (1.0f + resolutionEffectScale * 0.5f)),
            screenSettings.maskDepth,
          });

        if (screenSettings.diffusionStrength > 0.0f)
        {
          RenderBlur(currentFrameRGBInput);
        }

        device->RenderQuad(
          ShaderID::CRT_RGBToCRT,
          outputTexture,
          {
            {currentFrameRGBInput, SamplerType::LinearClamp},
            {prevRGBInput.get(), SamplerType::LinearClamp},
            {screenTexture.get(), SamplerType::NearestClamp},
            {blurTexture.get(), SamplerType::LinearClamp},
          },
          rgbToScreenConstantBuffer.get());

        device->RenderQuad(
          ShaderID::Util_Copy,
          prevRGBInput.get(),
          { { currentFrameRGBInput, SamplerType::LinearClamp } });

        prevScanlineType = scanType;
      }

    private:
      static constexpr uint32_t k_maskSize = 512;

      struct AspectData
      {
        Vec2 overscanSize;
        float aspect;
      };


      struct CommonConstants
      {
        Vec2 viewScale;               // Scale to get the correct aspect ratio of the image
        Vec2 overscanScale;           // overscanSize / standardSize;
        Vec2 overscanOffset;          // How much texture space to offset the center of our coordinate for overscan
        Vec2 distortion;              // How much to distort the coordinate system for screen curvature (from 0 .. 1)
      };


      struct ScreenTextureConstants
      {
        CommonConstants common;

        Vec2 maskDistortion;          // Where to put the mask sides
        Vec2 maskScale;               // Scale of the mask texture lookup
        float screenAspect;
        float roundedCornerSize;      // 0 == no corner, 1 == screen is an oval
      };


      struct RGBToScreenConstants
      {
        CommonConstants common;

        Color backgroundColor;
        float phosphorPersistence;
        float scanlineCount;          // How many scanlines there are
        float scanlineStrength;       // How strong the scanlines are (0 == none, 1 == whoa)
        float curEvenOddTexelOffset;  // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
        float prevEvenOddTexelOffset; // This is 0.5 if it's an odd frame (or progressive) and -0.5 if it's even.
        float diffusionStrength;      // The strength of the diffusion blur that is blended into the signal.
        float maskStrength;           // How much to blend the mask in. 0 means "no mask" and 1 means "fully apply"
        float maskDepth;
      };


      struct GaussianBlurConstants
      {
        Vec2 blurDir;
      };


      struct ToneMapConstants
      {
        Vec2 downsampleDir;
        float minLuminosity;
        float colorPower;
      };


      AspectData CalculateAspectData()
      {
        // Figure out how to adjust our viewed texture area for overscan
        float overscanSizeX =
          float(originalInputImageWidth - (overscanSettings.overscanLeft + overscanSettings.overscanRight));
        float overscanSizeY = float(scanlineCount - (overscanSettings.overscanTop + overscanSettings.overscanBottom));
        return {
          { overscanSizeX, overscanSizeY },
          pixelAspect * overscanSizeX / overscanSizeY,
        };
      }


      CommonConstants CalculateCommonConstants(const AspectData &aspectData)
      {
        CommonConstants data;

        data.overscanScale.x = aspectData.overscanSize.x / originalInputImageWidth;
        data.overscanScale.y = aspectData.overscanSize.y / scanlineCount;

        data.overscanOffset.x = 0.5f * float(int32_t(overscanSettings.overscanLeft - overscanSettings.overscanRight))
          / originalInputImageWidth;
        data.overscanOffset.y = 0.5f * float(int32_t(overscanSettings.overscanTop - overscanSettings.overscanBottom))
          / scanlineCount;

        // Figure out the aspect ratio of the output, given both our dimensions as well as the pixel aspect ratio in
        //  the screen settings.
        if (float(screenTexture->Width()) > aspectData.aspect * float(screenTexture->Height()))
        {
          float desiredWidth = aspectData.aspect * float(screenTexture->Height());
          data.viewScale.x = float(screenTexture->Width()) / desiredWidth;
          data.viewScale.y = 1.0f;
        }
        else
        {
          float desiredHeight = float(screenTexture->Width()) / aspectData.aspect;
          data.viewScale.x = 1.0f;
          data.viewScale.y = float(screenTexture->Height()) / desiredHeight;
        }

        // Taking the square root of the distortion gives us a little more change at smaller values.
        data.distortion.x = std::sqrt(screenSettings.distortion.x);
        data.distortion.y = std::sqrt(screenSettings.distortion.y);

        return data;
      }


      void RenderScreenTexture()
      {
        assert(screenTexture != nullptr);

        ScreenTextureConstants data;

        auto aspectData = CalculateAspectData();
        data.common = CalculateCommonConstants(aspectData);
        data.maskDistortion.x = screenSettings.screenEdgeRounding.x * 0.5f;
        data.maskDistortion.y = screenSettings.screenEdgeRounding.y * 0.5f;
        data.roundedCornerSize = screenSettings.cornerRounding;

        data.maskScale.y = float(scanlineCount) * data.common.overscanScale.y * 0.5f / screenSettings.maskScale;
        data.maskScale.x = float(originalInputImageWidth) * pixelAspect * data.common.overscanScale.x * 0.25f
          / screenSettings.maskScale;

        switch (screenSettings.maskType)
        {
        case MaskType::SlotMask:
          data.maskScale.x *= 1.3f;
          data.maskScale.y *= 1.3f;
          break;

        case MaskType::ShadowMask:
          data.maskScale.x *= 0.6f;
          data.maskScale.y *= 0.6f;
          break;

        case MaskType::ApertureGrille:
          data.maskScale.x *= 1.4f;
          break;
        }

        // Between 4k and 2k (2160p and 1080p vertical resolution) we want to scale the mask a little larger to help it
        //  read better at lower-than-4k resolutions. The resulting scale values here were eyeballed to make the 2k
        //  version look reasonably consistent with the 4k one.
        float resolutionEffectScale = std::max(
          0.0f,
          std::min(1.0f, 1.0f - (float(screenTexture->Height()) - 1080.0f) / 1080.0f));
        data.maskScale.x *= (1.0f - 0.1f * resolutionEffectScale);
        data.maskScale.y *= (1.0f - 0.1f * resolutionEffectScale);

        data.screenAspect = aspectData.aspect;

        screenTextureConstantBuffer->Update(data);

        device->RenderQuad(
          ShaderID::CRT_GenerateScreenTexture,
          screenTexture.get(),
          {{maskTexture.get(), SamplerType::LinearWrap}},
          screenTextureConstantBuffer.get());
      }


      void UpdateBlurTextures()
      {
        auto aspectData = CalculateAspectData();
        uint32_t tonemapTexWidth;
        uint32_t tonemapTexHeight;
        uint32_t blurTextureWidth;
        if (float(processedRGBTextureWidth) > aspectData.aspect * float(scanlineCount))
        {
          // the tonemap texture is going to scale down to 2x the actual aspect we want (we'll downsample farther from
          //  there)
          tonemapTexHeight = scanlineCount;
          tonemapTexWidth = uint32_t(std::round(aspectData.aspect * float(scanlineCount))) * 2;
          blurTextureWidth = tonemapTexWidth / 2;
          downsampleDirX = 1.0f;
          downsampleDirY = 0.0f;
        }
        else
        {
          // This is an unlikely case (the signal already has a massive stretch), in this case we'll keep things the
          //  same and the blur texture will be scaled up slightly.
          tonemapTexWidth = originalInputImageWidth;
          tonemapTexHeight = scanlineCount;
          blurTextureWidth = uint32_t(std::round(aspectData.aspect * float(scanlineCount)));
          downsampleDirX = 0.0f;
          downsampleDirY = 1.0f;
        }

        if (toneMapTexture == nullptr
          || toneMapTexture->Width() != tonemapTexWidth
          || toneMapTexture->Height() != tonemapTexHeight)
        {
          // Rebuild our blur textures.
          toneMapTexture = device->CreateRenderTarget(
            tonemapTexWidth,
            tonemapTexHeight,
            1,
            TextureFormat::RGBA_Unorm8);

          blurTexture = device->CreateRenderTarget(
            blurTextureWidth,
            tonemapTexHeight,
            1,
            TextureFormat::RGBA_Unorm8);

          blurScratchTexture = device->CreateRenderTarget(
            blurTextureWidth,
            tonemapTexHeight,
            1,
            TextureFormat::RGBA_Unorm8);
        }
      }


      // Generate the mask texture we use for the CRT emulation
      void RenderMaskTexture()
      {
        ShaderID shader;
        switch (screenSettings.maskType)
        {
        case MaskType::SlotMask:
          shader = ShaderID::CRT_GenerateSlotMask;
          break;

        case MaskType::ShadowMask:
          shader = ShaderID::CRT_GenerateShadowMask;
          break;

        case MaskType::ApertureGrille:
        default:
          shader = ShaderID::CRT_GenerateApertureGrille;
          break;
        }

        // First step is the generate the texture at the largest mip level
        generateMaskConstantBuffer->Update(Vec2{ float(k_maskSize), float(k_maskSize / 2) });
        device->RenderQuad(
          shader,
          maskTexture.get(),
          {},
          generateMaskConstantBuffer.get());

        // Now it's generated so we need to generate the mips by using our lanczos downsample
        maskDownsampleConstantBufferH->Update(Vec2{ 1.0f, 0.0f });
        maskDownsampleConstantBufferV->Update(Vec2{ 0.0f, 1.0f });
        for (uint32_t destMip = 1; destMip < maskTexture->MipCount(); destMip++)
        {
          device->RenderQuad(
            ShaderID::Util_Downsample2X,
            {halfWidthMaskTexture.get(), destMip - 1},
            {{maskTexture.get(), destMip - 1, SamplerType::LinearWrap}},
            maskDownsampleConstantBufferH.get());

          device->RenderQuad(
            ShaderID::Util_Downsample2X,
            {maskTexture.get(), destMip},
            {{halfWidthMaskTexture.get(), destMip - 1, SamplerType::LinearWrap}},
            maskDownsampleConstantBufferV.get());
        }
      }


      void RenderBlur(const ITexture *inputTexture)
      {
        // $TODO: This is slightly inaccurate, we should really be using the max of inputTexture and
        //  prevFrameTexture * phosphorPersistence, but for now, this is fine.
        toneMapConstantBuffer->Update(
          ToneMapConstants {
            { downsampleDirX, downsampleDirY },

            // $TODO: Probably want to expose these values too, since everything else is an option
            0.0f,
            1.3f,
          });

        // We're downsampling "2x" horizontally (scare quotes because it isn't always exactly 2x but it's close enough
        //  that we can just abuse this shader as if it were)
        blurDownsampleConstantBuffer->Update(Vec2{ 1.0f, 0.0f });

        device->RenderQuad(
          ShaderID::Util_TonemapAndDownsample,
          toneMapTexture.get(),
          {{inputTexture, SamplerType::LinearClamp}},
          toneMapConstantBuffer.get());

        device->RenderQuad(
          ShaderID::Util_Downsample2X,
          blurTexture.get(),
          {{toneMapTexture.get(), SamplerType::LinearClamp}},
          blurDownsampleConstantBuffer.get());

        gaussianBlurConstantBufferH->Update(GaussianBlurConstants{1.0f, 0.0f});
        device->RenderQuad(
          ShaderID::Util_GaussianBlur13,
          blurScratchTexture.get(),
          {{blurTexture.get(), SamplerType::LinearClamp}},
          gaussianBlurConstantBufferH.get());

        gaussianBlurConstantBufferV->Update(GaussianBlurConstants{0.0f, 1.0f});
        device->RenderQuad(
          ShaderID::Util_GaussianBlur13,
          blurTexture.get(),
          {{blurScratchTexture.get(), SamplerType::LinearClamp}},
          gaussianBlurConstantBufferV.get());
      }


      IGraphicsDevice *device;

      uint32_t originalInputImageWidth;
      uint32_t processedRGBTextureWidth;
      uint32_t scanlineCount;
      float pixelAspect;
      bool isFirstFrame = true;

      std::unique_ptr<IConstantBuffer> screenTextureConstantBuffer;
      std::unique_ptr<IConstantBuffer> rgbToScreenConstantBuffer;
      std::unique_ptr<IConstantBuffer> toneMapConstantBuffer;
      std::unique_ptr<IConstantBuffer> blurDownsampleConstantBuffer;
      std::unique_ptr<IConstantBuffer> gaussianBlurConstantBufferH;
      std::unique_ptr<IConstantBuffer> gaussianBlurConstantBufferV;
      std::unique_ptr<IConstantBuffer> generateMaskConstantBuffer;
      std::unique_ptr<IConstantBuffer> maskDownsampleConstantBufferH;
      std::unique_ptr<IConstantBuffer> maskDownsampleConstantBufferV;

      std::unique_ptr<IRenderTarget> prevRGBInput;

      std::unique_ptr<IRenderTarget> maskTexture;
      std::unique_ptr<IRenderTarget> halfWidthMaskTexture;
      std::unique_ptr<IRenderTarget> screenTexture;

      std::unique_ptr<IRenderTarget> toneMapTexture;
      std::unique_ptr<IRenderTarget> blurScratchTexture;
      std::unique_ptr<IRenderTarget> blurTexture;

      ScreenSettings screenSettings;
      OverscanSettings overscanSettings;
      bool needsRenderScreenTexture = false;
      bool needsRenderMaskTexture = false;

      ScanlineType prevScanlineType = ScanlineType::Progressive;
      float downsampleDirX;
      float downsampleDirY;
    };
  }
}