#pragma once

#include "CathodeRetro/GraphicsDevice.h"
#include "CathodeRetro/Settings.h"

class IDemoHandler
{
public:
  virtual ~IDemoHandler() = default;

  virtual std::unique_ptr<CathodeRetro::ITexture> CreateRGBATexture(
    uint32_t width,
    uint32_t height,
    uint32_t *rgbaData) = 0;

  virtual void SetCathodeRetroSourceSettings(
      CathodeRetro::SignalType sigType,
      uint32_t inputWidth,
      uint32_t inputHeight,
      const CathodeRetro::SourceSettings &sourceSettings) = 0;

  virtual void UpdateCathodeRetroSettings(
      const CathodeRetro::ArtifactSettings &artifactSettings,
      const CathodeRetro::TVKnobSettings &knobSettings,
      const CathodeRetro::OverscanSettings &overscanSettings,
      const CathodeRetro::ScreenSettings &screenSettings) = 0;

  virtual void ResizeBackbuffer(uint32_t width, uint32_t height) = 0;

  virtual void Render(const CathodeRetro::ITexture *currentFrame, CathodeRetro::ScanlineType scanlineType) = 0;
};


