#define NOMINMAX
#include <Windows.h>
#include <CommCtrl.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <GL/GL.h>

#include "GLHelpers.h"
#include "GLGraphicsDevice.h"

#include "CathodeRetro/CathodeRetro.h"
#include "CathodeRetro/SettingPresets.h"

static HWND s_hwnd;
static HINSTANCE s_instance;
static HDC s_dc;
static bool s_fullscreen = false;
static RECT s_oldWindowedRect;

static std::thread s_renderThread;
static std::atomic<bool> s_stopRenderThread = false;
static std::mutex s_renderThreadMutex;

void ToggleFullscreen()
{
  s_fullscreen = !s_fullscreen;
  if (s_fullscreen)
  {
    GetWindowRect(s_hwnd, &s_oldWindowedRect);
    SetWindowLongPtr(s_hwnd, GWL_STYLE, GetWindowLongPtr(s_hwnd, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW | WS_POPUP);
    SetMenu(s_hwnd, nullptr);

    HMONITOR primaryMon = MonitorFromPoint({s_oldWindowedRect.left, s_oldWindowedRect.top}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(primaryMon, &info);

    int32_t width = info.rcMonitor.right - info.rcMonitor.left;
    int32_t height = info.rcMonitor.bottom - info.rcMonitor.top;
    SetWindowPos(s_hwnd, nullptr, info.rcMonitor.left, info.rcMonitor.top, width, height, SWP_NOZORDER);
  }
  else
  {
    SetWindowLongPtr(s_hwnd, GWL_STYLE, GetWindowLongPtr(s_hwnd, GWL_STYLE) & ~WS_POPUP| WS_OVERLAPPEDWINDOW);
    // Menu if needed
    SetWindowPos(
      s_hwnd,
      nullptr,
      s_oldWindowedRect.left,
      s_oldWindowedRect.top,
      s_oldWindowedRect.right - s_oldWindowedRect.left,
      s_oldWindowedRect.bottom - s_oldWindowedRect.top,
      SWP_NOZORDER);
  }
}


LRESULT FAR PASCAL WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
  switch( message )
  {
  case WM_CREATE:
    break;

  case WM_DESTROY:
    PostQuitMessage( 0 );
    break;

  case WM_ERASEBKGND:
    return TRUE;

  case WM_KEYDOWN:
    switch (wParam)
    {
     case VK_ESCAPE:
      ToggleFullscreen();
      break;
    }
    break;

  case WM_CLOSE:
    break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}



static void DoInit( HINSTANCE hInstance )
{
  HWND                hwnd;
  WNDCLASS wc;

  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = 0;
  wc.hCursor = LoadCursor( NULL, IDC_ARROW );
  wc.hbrBackground = CreateSolidBrush(0);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = L"Cathode Retro OpenGL Test App";
  RegisterClass( &wc );

  RECT screenRect;
  screenRect.left = 0;
  screenRect.right = 256*3;
  screenRect.top = 0;
  screenRect.bottom = 224*3;
  AdjustWindowRect(&screenRect, WS_OVERLAPPEDWINDOW, TRUE);

  s_instance = hInstance;

  hwnd = CreateWindowEx(
    0,
    L"Cathode Retro OpenGL Test App",
    L"Cathode Retro OpenGL Test App",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    screenRect.right - screenRect.left,
    screenRect.bottom - screenRect.top,
    nullptr,
    nullptr,
    hInstance,
    nullptr);
  s_hwnd = hwnd;
}


void SetGLPixelFormat(HWND hwnd)
{
  s_dc = GetDC(hwnd);

  PIXELFORMATDESCRIPTOR pfd = {
    .nSize = sizeof(PIXELFORMATDESCRIPTOR),
    .nVersion = 1,
    .dwFlags = PFD_DRAW_TO_WINDOW
       | PFD_SUPPORT_OPENGL
       | PFD_DOUBLEBUFFER,
    .iPixelType = PFD_TYPE_RGBA,
    .cColorBits = 24,
    .iLayerType = PFD_MAIN_PLANE,
  };

  GLint pixelFormat = ChoosePixelFormat(s_dc, &pfd);
  if (pixelFormat == 0)
  {
    throw std::exception("ChoosePixelFormat failed");
  }

  if (!SetPixelFormat(s_dc, pixelFormat, &pfd))
  {
    throw std::exception("SetPixelFormat failed");
  }
}


int CALLBACK WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
  try
  {
    if (CoInitialize(nullptr) != S_OK)
    {
      throw std::exception("Failed Initializing COM");
    }

    InitCommonControls();

    MSG msg;
    DoInit(hInstance);



    ShowWindow(s_hwnd, SW_NORMAL);

    SetGLPixelFormat(s_hwnd);

    GLGraphicsDevice device { s_dc };
    auto rt = device.CreateRenderTarget(256, 240, 0, CathodeRetro::TextureFormat::RGBA_Unorm8);

    CathodeRetro::CathodeRetro cathodeRetro(
      &device,
      CathodeRetro::SignalType::RGB,
      rt->Width(),
      rt->Height(),
      CathodeRetro::k_sourcePresets[0].settings,
      CathodeRetro::k_artifactPresets[0].settings,
      {},
      {},
      CathodeRetro::k_screenPresets[3].settings);

    for (;;)
    {
      bool done = false;
      while(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
      {
        if(!GetMessage(&msg, NULL, 0, 0))
        {
          done = true;
          break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      if (done)
      {
        break;
      }

      glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLTexture *>(rt.get())->FBOHandle(0));
      glViewport(0, 0, rt->Width(), rt->Height());
      glClearColor(0.2f, 0.5f, 1.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      RECT r;
      GetClientRect(s_hwnd, &r);
      GLTexture backbuffer = GLTexture::FromBackbuffer(r.right, r.bottom);

      cathodeRetro.SetOutputSize(r.right, r.bottom);
      cathodeRetro.Render(rt.get(), rt.get(), CathodeRetro::ScanlineType::Progressive, &backbuffer);
      SwapBuffers(s_dc);
    }
  }
  catch (const std::exception &ex)
  {
    MessageBoxA(s_hwnd, ex.what(), "Error", MB_OK);
  }


  return 0;
}
