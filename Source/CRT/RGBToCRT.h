#pragma once

#include <cinttypes>
#include "CRT/ScreenSettings.h"
#include "GraphicsDevice.h"
#include "ProcessContext.h"
#include "resource.h"
#include "Util.h"

namespace NTSCify::CRT
{
  // This class takes RGB data (either the input or SVideo/composite filtering final output) and draws it as if it were on a CRT screen
  class RGBToCRT
  {
  public:
    RGBToCRT(
      GraphicsDevice *device, 
      uint32_t inputImageWidthIn, 
      uint32_t signalTextureWidthIn, 
      uint32_t scanlineCountIn)
    : inputImageWidth(inputImageWidthIn)
    , signalTextureWidth(signalTextureWidthIn)
    , scanlineCount(scanlineCountIn)
    {
      device->CreateTexture2D(
        signalTextureWidth,
        scanlineCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        GraphicsDevice::TextureFlags::UAV,
        &prevFrameTex.texture,
        &prevFrameTex.srv,
        &prevFrameTex.uav);

      device->CreatePixelShader(IDR_RGB_TO_CRT, &rgbToScreenShader);
      device->CreateConstantBuffer(sizeof(RGBToScreenConstants), &constantBuffer);

      GenerateShadowMaskTexture(device);
    }


    void Render(
      GraphicsDevice *device, 
      ProcessContext *processContext,
      const ScreenSettings &screenSettings)
    {
      auto deviceContext = device->Context();

      auto outputTarget = device->BackbufferTexture();
      uint32_t outputTargetWidth;
      uint32_t outputTargetHeight;

      // Set our rendertarget to render to backbuffer.
      // $TODO: Should probably make this take the target as a parameter since not everyone will be rendering to backbuffer
      {
        auto ptr = device->BackbufferView();
        deviceContext->OMSetRenderTargets(1, &ptr, nullptr);
        {
          D3D11_TEXTURE2D_DESC desc;
          outputTarget->GetDesc(&desc);
          outputTargetWidth = desc.Width;
          outputTargetHeight = desc.Height;
        }


        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = float(outputTargetWidth);
        vp.Height = float(outputTargetHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;

        deviceContext->RSSetViewports(1, &vp);
      }

      // Next up: Set up our shader constants
      {
        RGBToScreenConstants data;

        // Figure out how to adjust our viewed texture area for overscan
        float overscanSizeX = (inputImageWidth - float(screenSettings.overscanLeft + screenSettings.overscanRight));
        float overscanSizeY = (scanlineCount - float(screenSettings.overscanTop + screenSettings.overscanBottom));

        data.overscanScaleX = overscanSizeX / inputImageWidth;
        data.overscanScaleY = overscanSizeY / scanlineCount;

        data.overscanOffsetX = float(int32_t(screenSettings.overscanLeft - screenSettings.overscanRight)) / inputImageWidth * 0.5f;
        data.overscanOffsetY = float(int32_t(screenSettings.overscanTop - screenSettings.overscanBottom)) / scanlineCount * 0.5f;

        // Figure out the aspect ratio of the output, given both our dimensions as well as the pixel aspect ratio in the screen settings.
        {
          float inputPixelRatio = screenSettings.inputPixelAspectRatio;
          const float k_aspect = inputPixelRatio * overscanSizeX / overscanSizeY;
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
        }

        // 
        data.distortionX = screenSettings.horizontalDistortion;
        data.distortionY = screenSettings.verticalDistortion;
        data.maskDistortionX = screenSettings.screenEdgeRoundingX + data.distortionX;
        data.maskDistortionY = screenSettings.screenEdgeRoundingY + data.distortionY;
        data.roundedCornerSize = screenSettings.cornerRounding;

        // The values for shadowMaskScale were initially normalized against a 240-pixel-tall screen so just pretend it's ALWAYS that height for purposes
        //  of scaling the shadow mask.
        static constexpr float shadowMaskScaleNormalization = 240.0f * 0.7f;

        data.shadowMaskScaleX = float (inputImageWidth) / float(scanlineCount) 
                              * screenSettings.inputPixelAspectRatio 
                              * shadowMaskScaleNormalization 
                              * 0.45f 
                              * screenSettings.shadowMaskScale;
        data.shadowMaskScaleY = shadowMaskScaleNormalization * screenSettings.shadowMaskScale;
        data.shadowMaskStrength = screenSettings.shadowMaskStrength;
        data.phosphorDecay = screenSettings.phosphorDecay;

        data.scanlineCount = float(scanlineCount);
        data.scanlineStrength = screenSettings.scanlineStrength;

        data.signalTextureWidth = float(signalTextureWidth);

        device->DiscardAndUpdateBuffer(constantBuffer, &data);
      }

      // Now set up all the stuff we need to actually run the shader
      {
        float color[] = {0.0f, 0.0f, 0.0f, 0.0f};
        deviceContext->OMSetBlendState(processContext->blendState, color, 0xFFFFFFFF);
        deviceContext->RSSetState(processContext->rasterizerState);
        deviceContext->IASetInputLayout(processContext->inputLayout);
        deviceContext->VSSetShader(processContext->vertexShader, nullptr, 0);
        deviceContext->PSSetShader(rgbToScreenShader, nullptr, 0);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        {
          ID3D11SamplerState *st[] = {processContext->samplerStateClamp, processContext->samplerStateWrap};
          deviceContext->PSSetSamplers(0, UINT(k_arrayLength<decltype(st)>), st);
        }

        {
          auto ptr = processContext->vertexBuffer.Ptr();
          UINT stride = UINT(processContext->vertexSize);
          UINT offsets = 0;
          deviceContext->IASetVertexBuffers(0, 1, &ptr, &stride, &offsets);
        }
      }

      // Draw the actual output!
      {
        ID3D11ShaderResourceView *res[] = {processContext->colorTex.srv.Ptr(), prevFrameTex.srv.Ptr(), shadowMaskSRV.Ptr()};
        deviceContext->PSSetShaderResources(0, UINT(k_arrayLength<decltype(res)>), res);

        ID3D11Buffer *cb[] = {constantBuffer};
        deviceContext->PSSetConstantBuffers(0, UINT(k_arrayLength<decltype(cb)>), cb);

        deviceContext->Draw(6, 0);

        ZeroType(&res);
        deviceContext->PSSetShaderResources(0, UINT(k_arrayLength<decltype(res)>), res);
      }

      // Swap the color texture we used with our previous frame texture so we have that ready for next time
      std::swap(processContext->colorTex, prevFrameTex);
    }

  protected:
    // Generate the shadow mask texture we use for the CRT emulation
    void GenerateShadowMaskTexture(GraphicsDevice *device)
    {
      // $TODO this texture could be pre-made and the one being generated here is WAY overkill for how tiny it shows up on-screen, but it does look nice!
      auto context = device->Context();
      static constexpr uint32_t k_size = 512;
      static constexpr uint32_t k_mipCount = 8;

      device->CreateTexture2D(
        k_size,
        k_size / 2,
        k_mipCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        GraphicsDevice::TextureFlags::UAV,
        &shadowMaskTexture,
        &shadowMaskSRV);

      // First step is the generate the texture at the largest mip level
      {
        ComPtr<ID3D11ComputeShader> generateShadowMaskShader;
        device->CreateComputeShader(IDR_GENERATE_SHADOW_MASK, &generateShadowMaskShader);

        struct GenerateShadowMaskConstants
        {
          float blackLevel;
          float coordinateScale;
        };

        GenerateShadowMaskConstants consts;
        consts.blackLevel = 0.0;
        consts.coordinateScale = 1.0f / float(k_size);

        ComPtr<ID3D11Buffer> constBuf;
        device->CreateConstantBuffer(
          sizeof(GenerateShadowMaskConstants),
          &constBuf);

        device->DiscardAndUpdateBuffer(constBuf, &consts, sizeof(consts));

        ComPtr<ID3D11UnorderedAccessView> tempUAV;
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          ZeroType(&desc);
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
          desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
          desc.Texture2D.MipSlice = 0;
          CHECK_HRESULT(device->D3DDevice()->CreateUnorderedAccessView(shadowMaskTexture, &desc, tempUAV.AddressForReplace()), "Create UAV");
        }

        auto uav = tempUAV.Ptr();
        auto cb = constBuf.Ptr();

        context->CSSetShader(generateShadowMaskShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);

        context->Dispatch(k_size / 8, k_size / 8, 1);

        uav = nullptr;
        cb = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);
      }

      // Now it's generated so we need to generate the mips by using our lanczos downsample
      ComPtr<ID3D11ComputeShader> downsampleShader;
      device->CreateComputeShader(IDR_DOWNSAMPLE_2X, &downsampleShader);
      for (uint32_t destMip = 1; destMip < k_mipCount; destMip++)
      {
        ComPtr<ID3D11UnorderedAccessView> tempUAV;
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          ZeroType(&desc);
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
          desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
          desc.Texture2D.MipSlice = destMip;
          CHECK_HRESULT(device->D3DDevice()->CreateUnorderedAccessView(shadowMaskTexture, &desc, tempUAV.AddressForReplace()), "Create UAV");
        }

        ComPtr<ID3D11ShaderResourceView> tempSRV;
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC desc;
          ZeroType(&desc);
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
          desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
          desc.Texture2D.MipLevels = 1;
          desc.Texture2D.MostDetailedMip = destMip - 1;
          CHECK_HRESULT(device->D3DDevice()->CreateShaderResourceView(shadowMaskTexture, &desc, tempSRV.AddressForReplace()), "Create SRV");
        }

        auto srv = tempSRV.Ptr();
        auto uav = tempUAV.Ptr();

        context->CSSetShader(downsampleShader, nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, 1, &srv);

        context->Dispatch((k_size >> destMip) / 8, (k_size >> destMip) / 8, 1);

        uav = nullptr;
        srv = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetShaderResources(0, 1, &srv);
      }
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

      float signalTextureWidth;     // The width (in texels) of the signal texture
    };

    uint32_t inputImageWidth;
    uint32_t signalTextureWidth;
    uint32_t scanlineCount;

    ComPtr<ID3D11Buffer> constantBuffer;
    ComPtr<ID3D11PixelShader> rgbToScreenShader;
  
    ProcessContext::TextureSetUAV prevFrameTex;

    ComPtr<ID3D11Texture2D> shadowMaskTexture;
    ComPtr<ID3D11ShaderResourceView> shadowMaskSRV;
  };
}