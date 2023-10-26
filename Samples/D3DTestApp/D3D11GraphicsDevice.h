#pragma once

#include <memory>

#include "CathodeRetro/GraphicsDevice.h"


class ID3D11GraphicsDevice : public CathodeRetro::IGraphicsDevice
{
public:
  virtual ~ID3D11GraphicsDevice() = default;

  static std::unique_ptr<ID3D11GraphicsDevice> Create(HWND hwnd);

  virtual std::unique_ptr<CathodeRetro::ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    CathodeRetro::TextureFormat format,
    void *initialDataTexels) = 0;

  virtual uint32_t BackbufferWidth() const = 0;
  virtual uint32_t BackbufferHeight() const = 0;

  virtual void UpdateWindowSize(uint32_t width, uint32_t height) = 0;
  virtual void ClearBackbuffer() = 0;
  virtual void Present() = 0;
};