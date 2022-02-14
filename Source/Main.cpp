#pragma once
#include <memory>

#include "NTSCify/RGBToCRT.h"
#include "NTSCify/SignalGenerator.h"
#include "NTSCify/SignalDecoder.h"
#include "GraphicsDevice.h"
#include "ReadWicTexture.h"


static HWND s_hwnd;
static bool s_fullscreen = false;
static RECT s_oldWindowedRect;
static std::unique_ptr<GraphicsDevice> s_graphicsDevice;


struct LoadedTexture 
{
  uint32_t width = 0;
  uint32_t height = 0;
  SimpleArray<uint32_t> data;

  std::unique_ptr<ITexture> texture;

  std::unique_ptr<NTSCify::SignalGenerator> signalGenerator;
  std::unique_ptr<NTSCify::SignalDecoder> signalDecoder;
  std::unique_ptr<NTSCify::RGBToCRT> rgbToCRT;
};

std::unique_ptr<LoadedTexture> loadedTexture;

NTSCify::SourceSettings generationInfo = NTSCify::k_NESLikeSourceSettings;

void LoadTexture(wchar_t *path)
{
  if (path == nullptr || path[0] == L'\0')
  {
    loadedTexture = nullptr;
    return;
  }

  std::unique_ptr<LoadedTexture> load = std::make_unique<LoadedTexture>();
  uint32_t width;
  uint32_t height;
  auto loaded = ReadWicTexture(path, &width, &height);

  load->width = width;
  load->height = height;
  load->data = std::move(loaded);

  load->texture = s_graphicsDevice->CreateTexture(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, TextureFlags::None, load->data.Ptr(), width * sizeof(uint32_t));

  load->signalGenerator = std::make_unique<NTSCify::SignalGenerator>(
    s_graphicsDevice.get(), 
    NTSCify::SignalType::Composite,
    width,
    height,
    generationInfo);
  load->signalDecoder = std::make_unique<NTSCify::SignalDecoder>(s_graphicsDevice.get(), load->signalGenerator->SignalProperties());
  load->rgbToCRT = std::make_unique<NTSCify::RGBToCRT>(s_graphicsDevice.get(), width, load->signalGenerator->SignalProperties().scanlineWidth, height);

  loadedTexture = std::move(load);
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
  case WM_KEYDOWN:
    switch (wParam)
    {
    case 'O':
      {
        wchar_t filename[1024] = {0};
        OPENFILENAME ofn;
        ZeroType(&ofn);
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = L"Images (*.jpg;*.jpeg;*.png)\0*.jpg;*.jpeg;*.png\0\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = DWORD(k_arrayLength<decltype(filename)>);
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileName(&ofn))
        {
          LoadTexture(filename);
        }
      }
      break;
      
     case VK_ESCAPE:
      {
        s_fullscreen = !s_fullscreen;
        if (s_fullscreen)
        {
          GetWindowRect(s_hwnd, &s_oldWindowedRect);
          SetWindowLongPtr(s_hwnd, GWL_STYLE, GetWindowLongPtr(s_hwnd, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW | WS_POPUP);
          SetMenu(s_hwnd, nullptr);

          HMONITOR primaryMon = MonitorFromPoint({s_oldWindowedRect.left, s_oldWindowedRect.top}, MONITOR_DEFAULTTOPRIMARY);
          MONITORINFO info;
          ZeroType(&info);
          info.cbSize = sizeof(MONITORINFO);
          GetMonitorInfo(primaryMon, &info);

          int32_t width = info.rcMonitor.right - info.rcMonitor.left;
          int32_t height = info.rcMonitor.bottom - info.rcMonitor.top;
          SetWindowPos(s_hwnd, nullptr, info.rcMonitor.left, info.rcMonitor.top, width, height, SWP_NOZORDER);
        }
        else
        {
          SetWindowLongPtr(s_hwnd, GWL_STYLE, GetWindowLongPtr(s_hwnd, GWL_STYLE) & ~WS_POPUP| WS_OVERLAPPEDWINDOW);
          SetWindowPos(
            s_hwnd, 
            nullptr, 
            s_oldWindowedRect.left, 
            s_oldWindowedRect.top, 
            s_oldWindowedRect.right - s_oldWindowedRect.left,
            s_oldWindowedRect.bottom - s_oldWindowedRect.top,
            SWP_NOZORDER);
        }

        s_graphicsDevice->UpdateWindowSize();
      }
      break;
    }
    break;
  case WM_SIZE:
    if (wParam != SIZE_MINIMIZED)
    {
      s_graphicsDevice->UpdateWindowSize(); 
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

  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = 0;
  wc.hCursor = LoadCursor( NULL, IDC_ARROW );
  wc.hbrBackground = CreateSolidBrush(0);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = L"NTSCify";
  RegisterClass( &wc );
  
  RECT screenRect;
  screenRect.left = 0;
  screenRect.right = 256*3;
  screenRect.top = 0;
  screenRect.bottom = 224*3;
  AdjustWindowRect(&screenRect, WS_OVERLAPPEDWINDOW, TRUE);

  hwnd = CreateWindowEx(
    0, 
    L"NTSCify", 
    L"NTSCify", 
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


int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
  CoInitialize(nullptr);

  try
  {
    MSG msg;
    DoInit(hInstance);

    s_graphicsDevice = std::make_unique<GraphicsDevice>(s_hwnd);

    ShowWindow(s_hwnd, SW_NORMAL);

    bool done = false;
    while (!done)
    {
      while(PeekMessage(&msg,NULL, 0, 0, PM_NOREMOVE))
      {
        if(!GetMessage(&msg, NULL, 0, 0))
        {
          done = true;
          break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      s_graphicsDevice->ClearBackbuffer();

      NTSCify::ArtifactSettings artifactSettings;
      artifactSettings.noiseStrength = 0.05f;
      artifactSettings.ghostVisibility = 0.0f; // 0.35f;
      artifactSettings.ghostSpreadScale = 0.71f;
      artifactSettings.ghostDistance = 3.1f;
      artifactSettings.temporalArtifactReduction = 1.0f;
      artifactSettings.instabilityScale = 0.5f;

      NTSCify::TVKnobSettings knobSettings;

      NTSCify::ScreenSettings screenSettings;
      screenSettings.inputPixelAspectRatio = 1.0f;
      screenSettings.horizontalDistortion = 0.20f;
      screenSettings.verticalDistortion = 0.25f;
      screenSettings.cornerRounding = 0.05f;
      screenSettings.shadowMaskScale = 1.0f;
      screenSettings.shadowMaskStrength = 0.8f;
      screenSettings.phosphorDecay = 0.05f;
      screenSettings.scanlineStrength = 0.25f;

      if (loadedTexture != nullptr)
      {
        loadedTexture->signalGenerator->SetArtifactSettings(artifactSettings);
        loadedTexture->signalGenerator->Generate(loadedTexture->texture.get());

        loadedTexture->signalDecoder->SetKnobSettings(knobSettings);
        loadedTexture->signalDecoder->Decode(loadedTexture->signalGenerator->SignalTexture(), loadedTexture->signalGenerator->PhasesTexture(), loadedTexture->signalGenerator->SignalLevels());

        loadedTexture->rgbToCRT->SetScreenSettings(screenSettings);
        loadedTexture->rgbToCRT->Render(loadedTexture->signalDecoder->CurrentFrameRGBOutput(), loadedTexture->signalDecoder->PreviousFrameRGBOutput());
      }

      s_graphicsDevice->Present();
    }
  }
  catch (const std::exception &ex)
  {
    MessageBoxA(s_hwnd, ex.what(), "Error", MB_OK);
  }

  return 0;
}
