#pragma once

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
    // This class takes RGB data (either the input or SVideo/composite filtering final output) and draws it as if it were on a CRT screen
    class RGBToCRT
    {
    public:
      RGBToCRT(
        IGraphicsDevice *deviceIn,
        uint32_t inputImageWidthIn,
        uint32_t signalTextureWidthIn,
        uint32_t scanlineCountIn,
        float pixelAspectIn)
      : device(deviceIn)
      , inputImageWidth(inputImageWidthIn)
      , signalTextureWidth(signalTextureWidthIn)
      , scanlineCount(scanlineCountIn)
      , pixelAspect(pixelAspectIn)
      {
        rgbToScreenShader = device->CreateShader(ShaderID::RGBToCRT);
        generateScreenTextureShader = device->CreateShader(ShaderID::GenerateScreenTexture);
        downsample2XShader = device->CreateShader(ShaderID::Downsample2X);
        gaussianBlurShader = device->CreateShader(ShaderID::GaussianBlur13);
        toneMapShader = device->CreateShader(ShaderID::TonemapAndDownsample);
        generateSlotMaskShader = device->CreateShader(ShaderID::GenerateSlotMask);
        generateShadowMaskShader = device->CreateShader(ShaderID::GenerateShadowMask);
        generateApertureGrilleShader = device->CreateShader(ShaderID::GenerateApertureGrille);

        screenTextureConstantBuffer = device->CreateConstantBuffer(sizeof(ScreenTextureConstants));
        rgbToScreenConstantBuffer = device->CreateConstantBuffer(sizeof(RGBToScreenConstants));
        toneMapConstantBuffer = device->CreateConstantBuffer(sizeof(ToneMapConstants));
        blurDownsampleConstantBuffer = device->CreateConstantBuffer(sizeof(Vec2));
        gaussianBlurConstantBufferH = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
        gaussianBlurConstantBufferV = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
        generateMaskConstantBuffer = device->CreateConstantBuffer(sizeof(Vec2));
        maskDownsampleConstantBufferH = device->CreateConstantBuffer(sizeof(Vec2));
        maskDownsampleConstantBufferV = device->CreateConstantBuffer(sizeof(Vec2));

        maskTexture = device->CreateRenderTarget(k_maskSize, k_maskSize / 2, 0, TextureFormat::RGBA_Unorm8);
        halfWidthMaskTexture = device->CreateRenderTarget(k_maskSize / 2, k_maskSize / 2, 0, TextureFormat::RGBA_Unorm8);

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


      void Render(const ITexture *currentFrameRGBInput, const ITexture *previousFrameRGBInput, ITexture *outputTexture, ScanlineType scanType)
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

        rgbToScreenConstantBuffer->Update(
          RGBToScreenConstants{
            CalculateCommonConstants(CalculateAspectData()),
            screenSettings.borderColor,

            // $TODO: may want to artificially increase phosphorPersistence if we're interlaced
            screenSettings.phosphorPersistence,
            float(scanlineCount),
            screenSettings.scanlineStrength,
            (scanType != ScanlineType::Even) ? 0.5f : -0.5f,
            (prevScanlineType != ScanlineType::Even) ? 0.5f : -0.5f,
            screenSettings.diffusionStrength,
            screenSettings.maskStrength,
            screenSettings.maskDepth,
          });

        if (screenSettings.diffusionStrength > 0.0f)
        {
          RenderBlur(currentFrameRGBInput);
        }

        device->RenderQuad(
          rgbToScreenShader.get(),
          outputTexture,
          {
            {currentFrameRGBInput, SamplerType::LinearClamp},
            {previousFrameRGBInput, SamplerType::LinearClamp},
            {screenTexture.get(), SamplerType::NearestClamp},
            {blurTexture.get(), SamplerType::LinearClamp},
          },
          rgbToScreenConstantBuffer.get());

        prevScanlineType = scanType;
      }

    protected:
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
        Vec2 overscanOffset;          // How much texture space to offset the center of our coordinate system due to overscan
        Vec2 distortion;              // How much to distort (from 0 .. 1)
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
        float overscanSizeX = float(inputImageWidth - (overscanSettings.overscanLeft + overscanSettings.overscanRight));
        float overscanSizeY = float(scanlineCount - (overscanSettings.overscanTop + overscanSettings.overscanBottom));
        return {
          overscanSizeX,
          overscanSizeY,
          pixelAspect * overscanSizeX / overscanSizeY,
        };
      }


      CommonConstants CalculateCommonConstants(const AspectData &aspectData)
      {
        CommonConstants data;

        data.overscanScale.x = aspectData.overscanSize.x / inputImageWidth;
        data.overscanScale.y = aspectData.overscanSize.y / scanlineCount;

        data.overscanOffset.x = float(int32_t(overscanSettings.overscanLeft - overscanSettings.overscanRight)) / inputImageWidth * 0.5f;
        data.overscanOffset.y = float(int32_t(overscanSettings.overscanTop - overscanSettings.overscanBottom)) / scanlineCount * 0.5f;

        // Figure out the aspect ratio of the output, given both our dimensions as well as the pixel aspect ratio in the screen settings.
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

        // The values for maskScale were initially normalized against a 240-line-tall screen so just pretend it's ALWAYS that height
        //  for purposes of scaling the mask.
        static constexpr float maskScaleNormalization = 240.0f * 0.7f;

        data.maskScale.x = float (inputImageWidth) / float(scanlineCount)
                          * pixelAspect
                          * maskScaleNormalization
                          * 0.45f
                          / screenSettings.maskScale;
        data.maskScale.y = maskScaleNormalization / screenSettings.maskScale;

        if (screenSettings.maskType == MaskType::ShadowMask)
        {
          data.maskScale.x /= 2.0f * 5.0f / 6.0f;
          data.maskScale.y /= 2.0f;
        }

        data.screenAspect = aspectData.aspect;

        screenTextureConstantBuffer->Update(data);

        device->RenderQuad(
          generateScreenTextureShader.get(),
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
        IShader *shader = nullptr;
        switch (screenSettings.maskType)
        {
        case MaskType::SlotMask:
          shader = generateSlotMaskShader.get();
          break;

        case MaskType::ShadowMask:
          shader = generateShadowMaskShader.get();
          break;

        case MaskType::ApertureGrille:
          shader = generateApertureGrilleShader.get();
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
            downsample2XShader.get(),
            {halfWidthMaskTexture.get(), destMip - 1},
            {{maskTexture.get(), destMip - 1, SamplerType::LinearWrap}},
            maskDownsampleConstantBufferH.get());

          device->RenderQuad(
            downsample2XShader.get(),
            {maskTexture.get(), destMip},
            {{halfWidthMaskTexture.get(), destMip - 1, SamplerType::LinearWrap}},
            maskDownsampleConstantBufferV.get());
        }
      }


      void RenderBlur(const ITexture *inputTexture)
      {
        // $TODO: This is slightly inaccurate, we should really be using the max of inputTexture and prevFrameTexture * phosphorPersistence
        //  but for now, this is fine.
        toneMapConstantBuffer->Update(
          ToneMapConstants {
            downsampleDirX,
            downsampleDirY,

            // $TODO: Probably want to expose these values too, since everything else is an option
            0.0f,
            1.3f,
          });

        // We're downsampling "2x" horizontally (scare quotes because it isn't always exactly 2x but it's close enough that we can just
        //  abuse this shader as if it were)
        blurDownsampleConstantBuffer->Update(Vec2{ 1.0f, 0.0f });

        device->RenderQuad(
          toneMapShader.get(),
          toneMapTexture.get(),
          {{inputTexture, SamplerType::LinearClamp}},
          toneMapConstantBuffer.get());

        device->RenderQuad(
          downsample2XShader.get(),
          blurTexture.get(),
          {{toneMapTexture.get(), SamplerType::LinearClamp}},
          blurDownsampleConstantBuffer.get());

        gaussianBlurConstantBufferH->Update(GaussianBlurConstants{1.0f, 0.0f});
        device->RenderQuad(
          gaussianBlurShader.get(),
          blurScratchTexture.get(),
          {{blurTexture.get(), SamplerType::LinearClamp}},
          gaussianBlurConstantBufferH.get());

        gaussianBlurConstantBufferV->Update(GaussianBlurConstants{0.0f, 1.0f});
        device->RenderQuad(
          gaussianBlurShader.get(),
          blurTexture.get(),
          {{blurScratchTexture.get(), SamplerType::LinearClamp}},
          gaussianBlurConstantBufferV.get());
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
      std::unique_ptr<IConstantBuffer> generateMaskConstantBuffer;
      std::unique_ptr<IConstantBuffer> maskDownsampleConstantBufferH;
      std::unique_ptr<IConstantBuffer> maskDownsampleConstantBufferV;

      std::unique_ptr<IShader> rgbToScreenShader;
      std::unique_ptr<IShader> downsample2XShader;
      std::unique_ptr<IShader> toneMapShader;
      std::unique_ptr<IShader> gaussianBlurShader;
      std::unique_ptr<IShader> generateScreenTextureShader;
      std::unique_ptr<IShader> generateSlotMaskShader;
      std::unique_ptr<IShader> generateShadowMaskShader;
      std::unique_ptr<IShader> generateApertureGrilleShader;

      std::unique_ptr<ITexture> maskTexture;
      std::unique_ptr<ITexture> halfWidthMaskTexture;
      std::unique_ptr<ITexture> screenTexture;

      std::unique_ptr<ITexture> toneMapTexture;
      std::unique_ptr<ITexture> blurScratchTexture;
      std::unique_ptr<ITexture> blurTexture;

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