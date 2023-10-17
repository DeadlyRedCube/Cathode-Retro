#pragma once

#include <memory>

#include "NTSCify/GraphicsDevice.h"


class ID3D11GraphicsDevice : public NTSCify::IGraphicsDevice
{
public:
  virtual ~ID3D11GraphicsDevice() = default;

  static std::unique_ptr<ID3D11GraphicsDevice> Create(HWND hwnd);

  virtual std::unique_ptr<NTSCify::ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    NTSCify::TextureFormat format,
    void *initialDataTexels) = 0;

  virtual uint32_t BackbufferWidth() const = 0;
  virtual uint32_t BackbufferHeight() const = 0;

  virtual void UpdateWindowSize() = 0;
  virtual void ClearBackbuffer() = 0;
  virtual void Present() = 0;
};