#pragma once

#define NOMINMAX
#include <Windows.h>

#include "CathodeRetro/CathodeRetro.h"

#include "DemoHandler.h"
#include "D3D11GraphicsDevice.h"

class D3DDemoHandler : public IDemoHandler
{
public:
  D3DDemoHandler(HWND hwnd)
  {
    SetWindowTextW(hwnd, L"Cathode Retro D3D Demo");
    graphicsDevice = std::make_unique<D3D11GraphicsDevice>(hwnd);
  }

  std::unique_ptr<CathodeRetro::ITexture> CreateRGBATexture(uint32_t width, uint32_t height, uint32_t *rgbaData) override
  {
    return graphicsDevice->CreateTexture(width, height, CathodeRetro::TextureFormat::RGBA_Unorm8, rgbaData);
  }

  void SetCathodeRetroSourceSettings(
    CathodeRetro::SignalType sigType,
    uint32_t inputWidth,
    uint32_t inputHeight,
    const CathodeRetro::SourceSettings &sourceSettings) override
  {
    if (cathodeRetro == nullptr)
    {
      cathodeRetro = std::make_unique<CathodeRetro::CathodeRetro>(
        graphicsDevice.get(),
        sigType,
        inputWidth,
        inputHeight,
        sourceSettings);
    }
    else
    {
      cathodeRetro->UpdateSourceSettings(sigType, inputWidth, inputHeight, sourceSettings);
    }
  }

  void UpdateCathodeRetroSettings(
    const CathodeRetro::ArtifactSettings &artifactSettings,
    const CathodeRetro::TVKnobSettings &knobSettings,
    const CathodeRetro::OverscanSettings &overscanSettings,
    const CathodeRetro::ScreenSettings &screenSettings) override
  {
    assert (cathodeRetro != nullptr);
    cathodeRetro->UpdateSettings(artifactSettings, knobSettings, overscanSettings, screenSettings);
  }

  void ResizeBackbuffer(uint32_t width, uint32_t height) override
  {
    graphicsDevice->UpdateWindowSize(width, height);
    cathodeRetro->SetOutputSize(width, height);
  }



  void Render(
    const CathodeRetro::ITexture *currentFrame,
    const CathodeRetro::ITexture *prevFrame,
    CathodeRetro::ScanlineType scanlineType) override
  {
    cathodeRetro->Render(currentFrame, prevFrame, scanlineType, nullptr);
    graphicsDevice->Present();
  }

private:
  std::unique_ptr<CathodeRetro::CathodeRetro> cathodeRetro;
  std::unique_ptr<D3D11GraphicsDevice> graphicsDevice;
};


// This function is called by the main demo code to create our demo handler
std::unique_ptr<IDemoHandler> MakeDemoHandler(HWND hwnd)
{
  return std::make_unique<D3DDemoHandler>(hwnd);
}
