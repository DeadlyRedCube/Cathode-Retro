#pragma once

#include <cinttypes>
#include "GraphicsDevice.h"
#include "NTSCRenderBuffers.h"
#include "resource.h"
#include "Util.h"

struct ScreenParameters
{
  float inputPixelAspectRatio = 1.0f; // 1/1 for square input pixels, but for the NES it's 8/7

  uint32_t overscanLeft = 0;
  uint32_t overscanRight = 0;
  uint32_t overscanTop = 0;
  uint32_t overscanBottom = 0;

  float horizontalDistortion = 0; // 0.40f;
  float verticalDistortion = 0; //0.20f;

  float screenEdgeRoundingX = 0.0f;
  float screenEdgeRoundingY = 0.0f;

  float cornerRounding = 0; // 0.05f;

  float shadowMaskScale = 1.1f;
  float shadowMaskStrength = 0.85f;

  float phosphorDecay = 0.25f;

  float scanlineStrength = 0.5f;

  float instabilityScale = 2.5f;
};


class CRTEmulation
{
public:
  CRTEmulation(
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
      &prevFrameTexture,
      &prevFrameSRV,
      &prevFrameUAV);

    device->CreatePixelShader(IDR_RGBTOSCREEN, &rgbToScreenShader);
    device->CreateConstantBuffer(sizeof(RGBToScreenConstants), &constantBuffer);

    GenerateShadowMaskTexture(device);
  }


  void Render(
    GraphicsDevice *device, 
    NTSCRenderBuffers *buffers,
    const ScreenParameters &screenParameters)
  {
    auto context = device->Context();

    auto backbuffer = device->BackbufferTexture();

    {
      auto ptr = device->BackbufferView();
      context->OMSetRenderTargets(1, &ptr, nullptr);
    }

    {
      D3D11_TEXTURE2D_DESC desc;
      backbuffer->GetDesc(&desc);

      D3D11_VIEWPORT vp;
      vp.TopLeftX = 0.0f;
      vp.TopLeftY = 0.0f;
      vp.Width = float(desc.Width);
      vp.Height = float(desc.Height);
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;

      context->RSSetViewports(1, &vp);

      RGBToScreenConstants data;

      float overscanSizeX = (inputImageWidth - float(screenParameters.overscanLeft + screenParameters.overscanRight));
      float overscanSizeY = (scanlineCount - float(screenParameters.overscanTop + screenParameters.overscanBottom));

      data.overscanScaleX = overscanSizeX / inputImageWidth;
      data.overscanScaleY = overscanSizeY / scanlineCount;

      data.overscanOffsetX = float(int32_t(screenParameters.overscanLeft - screenParameters.overscanRight)) / inputImageWidth * 0.5f;
      data.overscanOffsetY = float(int32_t(screenParameters.overscanTop - screenParameters.overscanBottom)) / scanlineCount * 0.5f;

      {
        float inputPixelRatio = screenParameters.inputPixelAspectRatio;
        const float k_aspect = inputPixelRatio * overscanSizeX / overscanSizeY;
        if (float(desc.Width) > k_aspect * float(desc.Height))
        {
          float desiredWidth = k_aspect * float(desc.Height);
          data.viewScaleX = float(desc.Width) / desiredWidth;
          data.viewScaleY = 1.0f;
        }
        else
        {
          float desiredHeight = float(desc.Width) / k_aspect;
          data.viewScaleX = 1.0f;
          data.viewScaleY = float(desc.Height) / desiredHeight;
        }
      }

      data.distortionX = screenParameters.horizontalDistortion;
      data.distortionY = screenParameters.verticalDistortion;
      data.maskDistortionX = screenParameters.screenEdgeRoundingX + data.distortionX;
      data.maskDistortionY = screenParameters.screenEdgeRoundingY + data.distortionY;
      data.roundedCornerSize = screenParameters.cornerRounding;

      // The values for shadowMaskScale were normalized against a 240-pixel-tall screen so just pretend it's ALWAYS that height for purposes
      //  of scaling the shadow mask.
      static constexpr float shadowMaskScaleNormalization = 240.0f * 1.4f;

      data.shadowMaskScaleX = float (inputImageWidth) / float(scanlineCount) * screenParameters.inputPixelAspectRatio * shadowMaskScaleNormalization * 0.45f * screenParameters.shadowMaskScale;
      data.shadowMaskScaleY = shadowMaskScaleNormalization * screenParameters.shadowMaskScale;
      data.shadowMaskStrength = screenParameters.shadowMaskStrength;
      data.phosphorDecay = screenParameters.phosphorDecay;

      data.scanlineCount = float(scanlineCount);
      data.scanlineStrength = screenParameters.scanlineStrength;

      data.signalTextureWidth = float(signalTextureWidth);
      data.noiseSeed = noiseSeed;
      data.instabilityScale = screenParameters.instabilityScale;

      device->DiscardAndUpdateBuffer(constantBuffer, &data);
    }


    {
      float color[] = {0.0f, 0.0f, 0.0f, 0.0f};
      context->OMSetBlendState(buffers->blendState, color, 0xFFFFFFFF);
      context->RSSetState(buffers->rasterizerState);
      context->IASetInputLayout(buffers->inputLayout);
      context->VSSetShader(buffers->vertexShader, nullptr, 0);
      context->PSSetShader(rgbToScreenShader, nullptr, 0);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      {
        ID3D11SamplerState *st[] = {buffers->samplerStateClamp, buffers->samplerStateWrap};
        context->PSSetSamplers(0, UINT(k_arrayLength<decltype(st)>), st);
      }

      {
        auto ptr = buffers->vertexBuffer.Ptr();
        UINT stride = UINT(buffers->vertexSize);
        UINT offsets = 0;
        context->IASetVertexBuffers(0, 1, &ptr, &stride, &offsets);
      }
    }

    {
      ID3D11ShaderResourceView *res[] = {buffers->signalSRVColorA.Ptr(), prevFrameSRV.Ptr(), shadowMaskSRV.Ptr()};
      context->PSSetShaderResources(0, UINT(k_arrayLength<decltype(res)>), res);

      ID3D11Buffer *cb[] = {constantBuffer};
      context->PSSetConstantBuffers(0, UINT(k_arrayLength<decltype(cb)>), cb);

      context->Draw(6, 0);

      ZeroType(&res);
      context->PSSetShaderResources(0, UINT(k_arrayLength<decltype(res)>), res);
    }

    std::swap(buffers->signalTextureColorA, prevFrameTexture);
    std::swap(buffers->signalSRVColorA, prevFrameSRV);
    std::swap(buffers->signalUAVColorA, prevFrameUAV);

    noiseSeed = (noiseSeed + 1) % (60 * 60);
  }

protected:
  void GenerateShadowMaskTexture(GraphicsDevice *device)
  {
    // $TODO this texture could be pre-made and the one being generated here is WAY overkill for how large it shows up on-screen, but it does look nice!
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

    {
      ComPtr<ID3D11ComputeShader> generateShadowMaskShader;
      device->CreateComputeShader(IDR_GENERATESHADOWMASK, &generateShadowMaskShader);

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

    // Now it's generated so we need to generate the mips
    ComPtr<ID3D11ComputeShader> downsampleShader;
    device->CreateComputeShader(IDR_DOWNSAMPLE2X, &downsampleShader);
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

    float signalTextureWidth;
    int noiseSeed;
    float instabilityScale;
  };

  uint32_t inputImageWidth;
  uint32_t signalTextureWidth;
  uint32_t scanlineCount;

  ComPtr<ID3D11Buffer> constantBuffer;
  ComPtr<ID3D11PixelShader> rgbToScreenShader;
  
  ComPtr<ID3D11Texture2D> prevFrameTexture;
  ComPtr<ID3D11ShaderResourceView> prevFrameSRV;
  ComPtr<ID3D11UnorderedAccessView> prevFrameUAV;

  ComPtr<ID3D11Texture2D> shadowMaskTexture;
  ComPtr<ID3D11ShaderResourceView> shadowMaskSRV;

  int noiseSeed = 0;
};