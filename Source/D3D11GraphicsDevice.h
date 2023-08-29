#pragma once

#include <memory>

#define NOMINMAX
#include <Windows.h>

#include "NTSCify/GraphicsDevice.h"


class ID3D11GraphicsDevice : public NTSCify::IGraphicsDevice
{
public:
  virtual ~ID3D11GraphicsDevice() = default;

  static std::unique_ptr<ID3D11GraphicsDevice> Create(HWND hwnd);

  virtual void UpdateWindowSize() = 0;
  virtual void ClearBackbuffer() = 0;
  virtual void Present() = 0;

};