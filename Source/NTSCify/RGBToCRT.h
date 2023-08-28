#pragma once

#include <cassert>
#include <cinttypes>
#include "NTSCify/OverscanSettings.h"
#include "NTSCify/ScreenSettings.h"
#include "GraphicsDevice.h"
#include "resource.h"
#include "Util.h"

namespace NTSCify
{
  enum class ScanlineType
  {
    Progressive,     // Input scanline count is 1:1 with screen scanline count
    FauxProgressive, // Input scanline count is 1:2 with screen scanline count but the fields don't move (same positionas "Odd")
    Odd,             // This is an "odd" interlaced frame, the (1-based) odd scanlines will be full brightness.
    Even,            // This is an "even" interlaced frame
  };


  // This class takes RGB data (either the input or SVideo/composite filtering final output) and draws it as if it were on a CRT screen
  class RGBToCRT
  {
  public:
    RGBToCRT(
      GraphicsDevice *deviceIn, 
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
      rgbToScreenShader = device->CreatePixelShader(IDR_RGB_TO_CRT);
      generateScreenTextureShader = device->CreatePixelShader(IDR_GENERATE_SCREEN_TEXTURE);
      downsample2XShader = device->CreatePixelShader(IDR_DOWNSAMPLE_2X);
      gaussianBlurShader = device->CreatePixelShader(IDR_GAUSSIAN_BLUR_13);
      toneMapShader = device->CreatePixelShader(IDR_TONEMAP_AND_DOWNSAMPLE);
      rgbToScreenConstantBuffer = device->CreateConstantBuffer(sizeof(RGBToScreenConstants));
      samplePatternConstantBuffer = device->CreateConstantBuffer(sizeof(k_samplingPattern16X));
      toneMapConstantBuffer = device->CreateConstantBuffer(sizeof(ToneMapConstants));
      gaussianBlurConstantBufferH = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));
      gaussianBlurConstantBufferV = device->CreateConstantBuffer(sizeof(GaussianBlurConstants));

      GenerateShadowMaskTexture();
    }


    void SetScreenSettings(const ScreenSettings &settings)
    { 
      if (memcmp(&settings, &screenSettings, sizeof(ScreenSettings)) != 0)
      {
        screenSettings = settings; 
        screenSettingsDirty = true;
      }
    }


    void SetOverscanSettings(const OverscanSettings &settings)
    {
      overscanSettings = settings;
    }


    void Render(const ITexture *currentFrameRGBInput, const ITexture *previousFrameRGBInput, ITexture *outputTexture, ScanlineType scanType)
    {
      uint32_t outputTargetWidth = (outputTexture != nullptr) ? outputTexture->Width() : device->BackbufferWidth();
      uint32_t outputTargetHeight = (outputTexture != nullptr) ? outputTexture->Height() : device->BackbufferHeight();

      // Next up: Set up our shader constants
      {
        RGBToScreenConstants data;

        // Figure out how to adjust our viewed texture area for overscan
        float overscanSizeX = float(inputImageWidth - (overscanSettings.overscanLeft + overscanSettings.overscanRight));
        float overscanSizeY = float(scanlineCount - (overscanSettings.overscanTop + overscanSettings.overscanBottom));

        data.overscanScaleX = overscanSizeX / inputImageWidth;
        data.overscanScaleY = overscanSizeY / scanlineCount;

        data.overscanOffsetX = float(int32_t(overscanSettings.overscanLeft - overscanSettings.overscanRight)) / inputImageWidth * 0.5f;
        data.overscanOffsetY = float(int32_t(overscanSettings.overscanTop - overscanSettings.overscanBottom)) / scanlineCount * 0.5f;

        // Figure out the aspect ratio of the output, given both our dimensions as well as the pixel aspect ratio in the screen settings.
        {
          const float k_aspect = pixelAspect * overscanSizeX / overscanSizeY;
          if (float(outputTargetWidth) > k_aspect * float(outputTargetHeight))
          {
            float desiredWidth = k_aspect * float(outputTargetHeight);
            data.viewScaleX = float(outputTargetWidth) / desiredWidth;
            data.viewScaleY = 1.0f;
          }
          else
          {
            float desiredHeight = float(outputTargetWidth) / k_aspect;
            data.viewScaleX = 1.0f;
            data.viewScaleY = float(outputTargetHeight) / desiredHeight;
          }

          uint32_t tonemapTexWidth;
          uint32_t tonemapTexHeight;
          uint32_t blurTextureWidth;
          if (float(signalTextureWidth) > k_aspect * float(scanlineCount))
          {
            // the tonemap texture is going to scale down to 2x the actual aspect we want (we'll downsample farther from there)
            tonemapTexHeight = scanlineCount;
            tonemapTexWidth = uint32_t(std::round(k_aspect * float(scanlineCount))) * 2;
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
            blurTextureWidth = uint32_t(std::round(k_aspect * float(scanlineCount)));
            downsampleDirX = 0.0f;
            downsampleDirY = 1.0f;
          }

          if (toneMapTexture == nullptr || toneMapTexture->Width() != tonemapTexWidth || toneMapTexture->Height() != tonemapTexHeight)
          {
            // Rebuild our blur textures.
            toneMapTexture = device->CreateTexture(
              tonemapTexWidth,
              tonemapTexHeight,
              DXGI_FORMAT_R8G8B8A8_UNORM,
              TextureFlags::RenderTarget);

            blurTexture = device->CreateTexture(
              blurTextureWidth,
              tonemapTexHeight,
              DXGI_FORMAT_R8G8B8A8_UNORM,
              TextureFlags::RenderTarget);

            blurScratchTexture = device->CreateTexture(
              blurTextureWidth,
              tonemapTexHeight,
              DXGI_FORMAT_R8G8B8A8_UNORM,
              TextureFlags::RenderTarget);
          }
        }


        data.distortionX = screenSettings.horizontalDistortion;
        data.distortionY = screenSettings.verticalDistortion;
        data.maskDistortionX = screenSettings.screenEdgeRoundingX + data.distortionX;
        data.maskDistortionY = screenSettings.screenEdgeRoundingY + data.distortionY;
        data.roundedCornerSize = screenSettings.cornerRounding;

        // The values for shadowMaskScale were initially normalized against a 240-pixel-tall screen so just pretend it's ALWAYS that height for purposes
        //  of scaling the shadow mask.
        static constexpr float shadowMaskScaleNormalization = 240.0f * 0.7f;

        data.shadowMaskScaleX = float (inputImageWidth) / float(scanlineCount) 
                              * pixelAspect
                              * shadowMaskScaleNormalization 
                              * 0.45f 
                              / screenSettings.shadowMaskScale;
        data.shadowMaskScaleY = shadowMaskScaleNormalization / screenSettings.shadowMaskScale;
        data.shadowMaskStrength = screenSettings.shadowMaskStrength;

        // $TODO: may want to artificially increase phosphorDecay if we're interlaced
        data.phosphorDecay = screenSettings.phosphorDecay;

        data.scanlineCount = float(scanlineCount);
        data.scanlineStrength = (scanType != ScanlineType::Progressive) ? screenSettings.scanlineStrength : 0.0f;

        data.curEvenOddTexelOffset = (scanType != ScanlineType::Even) ? 0.5f : -0.5f;
        data.prevEvenOddTexelOffset = (m_prevScanlineType != ScanlineType::Even) ? 0.5f : -0.5f;

        data.diffusionStrength = screenSettings.diffusionStrength;

        device->DiscardAndUpdateBuffer(rgbToScreenConstantBuffer, &data);
      }

      if (screenSettingsDirty 
        || screenTexture == nullptr 
        || screenTexture->Width() != outputTargetWidth 
        || screenTexture->Height() != outputTargetHeight)
      {
        device->DiscardAndUpdateBuffer(samplePatternConstantBuffer, &k_samplingPattern16X);

        if (screenTexture == nullptr 
          || screenTexture->Width() != outputTargetWidth 
          || screenTexture->Height() != outputTargetHeight)
        {
          // Rebuild the texture at the correct resolution
          screenTexture = device->CreateTexture(outputTargetWidth, outputTargetHeight, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);
        }

        device->RenderQuadWithPixelShader(
          generateScreenTextureShader,
          screenTexture.get(),
          {shadowMaskTexture.get()},
          {SamplerType::Wrap},
          {rgbToScreenConstantBuffer, samplePatternConstantBuffer});

        screenSettingsDirty = false;
      }

      if (screenSettings.diffusionStrength > 0.0f)
      {
        RenderBlur(currentFrameRGBInput);
      }

      (void)previousFrameRGBInput;
      device->RenderQuadWithPixelShader(
        rgbToScreenShader,
        outputTexture,
        {currentFrameRGBInput, previousFrameRGBInput, screenTexture.get(), blurTexture.get()},
        {SamplerType::Clamp},
        {rgbToScreenConstantBuffer});

      m_prevScanlineType = scanType;
    }

  protected:
    // These are float2 sampling patterns, but they need to be float4 aligned for the constant buffers, so there's 2 padding floats per
    //  coordinate.
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


    // Generate the shadow mask texture we use for the CRT emulation
    void GenerateShadowMaskTexture()
    {
      // $TODO this texture could be pre-made and the one being generated here is WAY overkill for how tiny it shows up on-screen, but it does look nice!
      static constexpr uint32_t k_size = 512;
      static constexpr uint32_t k_mipCount = 8;

      ComPtr<ID3D11RenderTargetView> tempRTVLevel0;
      shadowMaskTexture = device->CreateTexture(k_size, k_size / 2, k_mipCount, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::RenderTarget);

      // First step is the generate the texture at the largest mip level
      {
        ComPtr<ID3D11PixelShader> generateShadowMaskShader = device->CreatePixelShader(IDR_GENERATE_SHADOW_MASK);

        struct GenerateShadowMaskConstants
        {
          float blackLevel;
          float coordinateScale;
          float texWidth;
          float texHeight;
        };

        GenerateShadowMaskConstants consts;
        consts.blackLevel = 0.0;
        consts.coordinateScale = 1.0f / float(k_size);
        consts.texWidth = float(k_size);
        consts.texHeight = float(k_size / 2);

        ComPtr<ID3D11Buffer> constBuf = device->CreateConstantBuffer(sizeof(GenerateShadowMaskConstants));

        device->DiscardAndUpdateBuffer(constBuf, &consts, sizeof(consts));

        device->RenderQuadWithPixelShader(
          generateShadowMaskShader,
          shadowMaskTexture.get(),
          {},
          {SamplerType::Clamp},
          {constBuf});
      }

      // Now it's generated so we need to generate the mips by using our lanczos downsample
      for (uint32_t destMip = 1; destMip < k_mipCount; destMip++)
      {
        device->RenderQuadWithPixelShader(
          downsample2XShader,
          {shadowMaskTexture.get(), destMip},
          {{shadowMaskTexture.get(), destMip - 1}},
          {SamplerType::Wrap},
          {});
      }
    }


    void RenderBlur(const ITexture *inputTexture)
    {
      // Step one: abuse the downsample2x shader to scale to whatever size we need.
      // $TODO: This is slightly inaccurate, we should really be using the max of inputTexture and prevFrameTexture * g_phosphorDecay but
      //  for now, this is fine.
      ToneMapConstants tmc;
      tmc.downsampleDirX = downsampleDirX;
      tmc.downsampleDirY = downsampleDirY;
      tmc.minLuminosity = 0.0f;
      tmc.colorPower = 1.3f;
      device->DiscardAndUpdateBuffer(toneMapConstantBuffer, &tmc);
      device->RenderQuadWithPixelShader(
        toneMapShader,
        toneMapTexture.get(),
        {inputTexture},
        {SamplerType::Clamp},
        {toneMapConstantBuffer});

      device->RenderQuadWithPixelShader(
        downsample2XShader,
        blurTexture.get(),
        {toneMapTexture.get()},
        {SamplerType::Clamp},
        {});

      GaussianBlurConstants blurH{1.0f, 0.0f};
      device->DiscardAndUpdateBuffer(gaussianBlurConstantBufferH, &blurH);
      device->RenderQuadWithPixelShader(
        gaussianBlurShader,
        blurScratchTexture.get(),
        {blurTexture.get()},
        {SamplerType::Clamp},
        {gaussianBlurConstantBufferH});

      GaussianBlurConstants blurV{0.0f, 1.0f};
      device->DiscardAndUpdateBuffer(gaussianBlurConstantBufferV, &blurV);
      device->RenderQuadWithPixelShader(
        gaussianBlurShader,
        blurTexture.get(),
        {blurScratchTexture.get()},
        {SamplerType::Clamp},
        {gaussianBlurConstantBufferV});
    }


    struct RGBToScreenConstants
    {
      float viewScaleX;             // Scale to get the correct aspect ratio of the image
      float viewScaleY;             // ...
      float overscanScaleX;         // overscanSize / standardSize;
      float overscanScaleY;         // ...
      float overscanOffsetX;        // How much texture space to offset the center of our coordinate system due to overscan
      float overscanOffsetY;        // ...
      float distortionX;            // How much to distort (from 0 .. 1)
      float distortionY;            // ...
      float maskDistortionX;        // Where to put the mask sides
      float maskDistortionY;        // ...

      float shadowMaskScaleX;       // Scale of the shadow mask texture lookup
      float shadowMaskScaleY;       // Scale of the shadow mask texture lookup
      float shadowMaskStrength;     // 
      float roundedCornerSize;      // 0 == no corner, 1 == screen is an oval
      float phosphorDecay;          // 
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
  

    GraphicsDevice *device;

    uint32_t inputImageWidth;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;
    float pixelAspect;

    ComPtr<ID3D11Buffer> rgbToScreenConstantBuffer;
    ComPtr<ID3D11Buffer> samplePatternConstantBuffer;
    ComPtr<ID3D11Buffer> toneMapConstantBuffer;
    ComPtr<ID3D11Buffer> gaussianBlurConstantBufferH;
    ComPtr<ID3D11Buffer> gaussianBlurConstantBufferV;
    ComPtr<ID3D11PixelShader> rgbToScreenShader;
    ComPtr<ID3D11PixelShader> downsample2XShader;
    ComPtr<ID3D11PixelShader> toneMapShader;
    ComPtr<ID3D11PixelShader> gaussianBlurShader;
    ComPtr<ID3D11PixelShader> generateScreenTextureShader;
  
    std::unique_ptr<ITexture> shadowMaskTexture;
    std::unique_ptr<ITexture> screenTexture;
    
    std::unique_ptr<ITexture> toneMapTexture;
    std::unique_ptr<ITexture> blurScratchTexture;
    std::unique_ptr<ITexture> blurTexture;

    ScreenSettings screenSettings;
    OverscanSettings overscanSettings;
    bool screenSettingsDirty = false;
    ScanlineType m_prevScanlineType = ScanlineType::Progressive;
    float downsampleDirX;
    float downsampleDirY;
  };
}