#pragma once

#define NOMINMAX
#include <Windows.h>

#include "CathodeRetro/CathodeRetro.h"

#include "DemoHandler.h"
#include "GLGraphicsDevice.h"

class GLDemoHandler : public IDemoHandler
{
public:
  GLDemoHandler(HWND hwnd)
  : windowDC(GetDC(hwnd))
  {
    SetWindowTextW(hwnd, L"Cathode Retro OpenGL Demo");

    // We need to set up the pixel format before initializing the graphics device
    PIXELFORMATDESCRIPTOR pfd =
    {
      .nSize = sizeof(PIXELFORMATDESCRIPTOR),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW
         | PFD_SUPPORT_OPENGL
         | PFD_DOUBLEBUFFER,
      .iPixelType = PFD_TYPE_RGBA,
      .cColorBits = 24,
      .iLayerType = PFD_MAIN_PLANE,
    };

    GLint pixelFormat = ChoosePixelFormat(windowDC, &pfd);
    if (pixelFormat == 0)
    {
      throw std::exception("ChoosePixelFormat failed");
    }

    if (!SetPixelFormat(windowDC, pixelFormat, &pfd))
    {
      throw std::exception("SetPixelFormat failed");
    }

    // Start by setting up an intermediate context
    wglContext = wglCreateContext(windowDC);
    if (wglContext == nullptr)
    {
      throw std::exception("wglCreateContext failed");
    }

    if (!wglMakeCurrent(windowDC, wglContext))
    {
      throw std::exception("wglMakeCurrent failed");
    }

    // Now that that's set, run InitializeGLHelpers which will load all of the GL 3.3 functions that we need to get this train rolling
    InitializeGLHelpers();

    // Now that that's done we can create our true context (With the correct ersion)
    int attribs[] =
    {
      WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
      WGL_CONTEXT_MINOR_VERSION_ARB, 3,
      0, 0,
    };

    // Create an OpenGL 3.3 context.
    wglContext = wglCreateContextAttribsARB(windowDC, nullptr, attribs);
    if (wglContext == nullptr)
    {
      throw std::exception("wglCreateContext failed");
    }

    if (!wglMakeCurrent(windowDC, wglContext))
    {
      throw std::exception("wglMakeCurrent failed");
    }

    // Now that we have a WGL context all set up we can actually set up the GL device.
    graphicsDevice = std::make_unique<GLGraphicsDevice>();
  }


  ~GLDemoHandler()
  {
    wglDeleteContext(wglContext);
  }


  virtual void ResizeBackbuffer(uint32_t width, uint32_t height)
  {
    if (width == outWidth && height == outHeight)
    {
      return;
    }

    outWidth = width;
    outHeight = height;

    // Any time the backbuffer is resized we need to notify cathodeRetro.
    cathodeRetro->SetOutputSize(width, height);

    // And we need a CathodeRetro::ITexture that represents the backbuffer to render to.
    backbuffer = std::make_unique<GLTexture>(GLTexture::FromBackbuffer(width, height));
  }


  std::unique_ptr<CathodeRetro::ITexture> CreateRGBATexture(uint32_t width, uint32_t height, uint32_t *rgbaData) override
  {
    // Demo app loads in images where 0, 0 is the upper-left corner, but GL's images put 0,0 in the lower right, so we need to vertically
    //  flip the image before creating the texture.
    std::vector<uint32_t> rgbaDataFlipped;
    rgbaDataFlipped.resize(width * height);
    for (size_t y = 0; y < height; y++)
    {
      size_t srcY = (height - 1 - y);
      memcpy(&rgbaDataFlipped[y * width], &rgbaData[srcY * width], width * sizeof(uint32_t));
    }

    return graphicsDevice->CreateTexture(width, height, CathodeRetro::TextureFormat::RGBA_Unorm8, rgbaDataFlipped.data());
  }


  void Render(
    const CathodeRetro::ITexture *currentFrame,
    const CathodeRetro::ITexture *prevFrame,
    CathodeRetro::ScanlineType scanlineType) override
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cathodeRetro->Render(currentFrame, prevFrame, scanlineType, backbuffer.get());

    SwapBuffers(windowDC);
  }


  void UpdateCathodeRetroSettings(
    CathodeRetro::SignalType sigType,
    uint32_t inputWidth,
    uint32_t inputHeight,
    const CathodeRetro::SourceSettings &sourceSettings,
    const CathodeRetro::ArtifactSettings &artifactSettings,
    const CathodeRetro::TVKnobSettings &knobSettings,
    const CathodeRetro::OverscanSettings &overscanSettings,
    const CathodeRetro::ScreenSettings &screenSettings) override
  {
    if (cathodeRetro == nullptr)
    {
      cathodeRetro = std::make_unique<CathodeRetro::CathodeRetro>(
        graphicsDevice.get(),
        sigType,
        inputWidth,
        inputHeight,
        sourceSettings,
        artifactSettings,
        knobSettings,
        overscanSettings,
        screenSettings);
    }
    else
    {
      cathodeRetro->UpdateSettings(
        sigType,
        inputWidth,
        inputHeight,
        sourceSettings,
        artifactSettings,
        knobSettings,
        overscanSettings,
        screenSettings);
    }
  }

private:
  HDC windowDC;
  HGLRC wglContext;
  uint32_t outWidth = 0;
  uint32_t outHeight = 0;

  std::unique_ptr<CathodeRetro::CathodeRetro> cathodeRetro;
  std::unique_ptr<GLGraphicsDevice> graphicsDevice;
  std::unique_ptr<GLTexture> backbuffer;
};


// This function is called by the main demo code to create our demo handler
std::unique_ptr<IDemoHandler> MakeDemoHandler(HWND hwnd)
{
  return std::make_unique<GLDemoHandler>(hwnd);
}
