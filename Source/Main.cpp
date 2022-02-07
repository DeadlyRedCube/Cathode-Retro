#pragma once
#include <memory>

#include "CRT/RGBToCRT.h"
#include "SignalGeneration/ApplyArtifacts.h"
#include "SignalGeneration/RGBToSVideo.h"
#include "SignalGeneration/SourceSettings.h"
#include "SignalGeneration/SVideoToComposite.h"
#include "SignalDecode/BlurIQ.h"
#include "SignalDecode/CompositeToSVideo.h"
#include "SignalDecode/FilterRGB.h"
#include "SignalDecode/SVideoToYIQ.h"
#include "SignalDecode/YIQToRGB.h"
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

  ComPtr<ID3D11Texture2D> texture;
  ComPtr<ID3D11ShaderResourceView> srv;

  std::unique_ptr<NTSCify::ProcessContext> processContext;
  std::unique_ptr<NTSCify::SignalGeneration::RGBToSVideo> rgbToSVideo;
  std::unique_ptr<NTSCify::SignalGeneration::ApplyArtifacts> applyArtifacts;
  std::unique_ptr<NTSCify::SignalGeneration::SVideoToComposite> sVideoToComposite;
  std::unique_ptr<NTSCify::SignalDecode::CompositeToSVideo> compositeToSVideo;
  std::unique_ptr<NTSCify::SignalDecode::SVideoToYIQ> sVideoToYIQ;
  std::unique_ptr<NTSCify::SignalDecode::BlurIQ> blurIQ;
  std::unique_ptr<NTSCify::SignalDecode::YIQToRGB> yiqToRGB;
  std::unique_ptr<NTSCify::SignalDecode::FilterRGB> filterRGB;
  std::unique_ptr<NTSCify::CRT::RGBToCRT> rgbToCRT;
};

std::unique_ptr<LoadedTexture> loadedTexture;

NTSCify::SignalGeneration::SourceSettings generationInfo = NTSCify::SignalGeneration::k_NESLikeSourceSettings;

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

  s_graphicsDevice->CreateTexture2D(
    width,
    height,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    GraphicsDevice::TextureFlags::None,
    &load->texture,
    &load->srv,
    nullptr,
    load->data.Ptr(),
    width * sizeof(uint32_t));

  load->processContext = std::make_unique<NTSCify::ProcessContext>(s_graphicsDevice.get(), width, height, generationInfo.colorCyclesPerInputPixel, generationInfo.denominator);
  load->rgbToSVideo = std::make_unique<NTSCify::SignalGeneration::RGBToSVideo>(
    s_graphicsDevice.get(),
    width,
    load->processContext->signalTextureWidth,
    height);
  load->applyArtifacts = std::make_unique<NTSCify::SignalGeneration::ApplyArtifacts>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->sVideoToComposite = std::make_unique<NTSCify::SignalGeneration::SVideoToComposite>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->compositeToSVideo = std::make_unique<NTSCify::SignalDecode::CompositeToSVideo>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->sVideoToYIQ = std::make_unique<NTSCify::SignalDecode::SVideoToYIQ>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->blurIQ = std::make_unique<NTSCify::SignalDecode::BlurIQ>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->yiqToRGB = std::make_unique<NTSCify::SignalDecode::YIQToRGB>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->filterRGB = std::make_unique<NTSCify::SignalDecode::FilterRGB>(s_graphicsDevice.get(), load->processContext->signalTextureWidth, height);
  load->rgbToCRT = std::make_unique<NTSCify::CRT::RGBToCRT>(s_graphicsDevice.get(), width, load->processContext->signalTextureWidth, height);

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
  auto kernel = CalculateGaussianKernel(25, 8.0f);

  CoInitialize(nullptr);

  try
  {
    MSG msg;
    DoInit(hInstance);

    s_graphicsDevice = std::make_unique<GraphicsDevice>(s_hwnd);

    ShowWindow(s_hwnd, SW_NORMAL);

    int32_t phase = 0;
    bool isEvenFrame = false;

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

      if (loadedTexture != nullptr)
      {
        loadedTexture->rgbToSVideo->Generate(
          s_graphicsDevice.get(),
          loadedTexture->srv,
          loadedTexture->processContext.get(),
          float(phase) / float(generationInfo.denominator),
          float(generationInfo.phaseIncrementPerLine) / float(generationInfo.denominator));

        isEvenFrame = !isEvenFrame;
        phase = (phase + (isEvenFrame ? generationInfo.phaseIncrementPerEvenFrame : generationInfo.phaseIncrementPerOddFrame)) % generationInfo.denominator;

        {
          NTSCify::SignalGeneration::ArtifactSettings options;
          options.noiseStrength = 0.025f;
          options.ghostVisibility = 0.0f; //0.65f;
          loadedTexture->applyArtifacts->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get(), options);
          loadedTexture->sVideoToComposite->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get());
          loadedTexture->compositeToSVideo->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get());
        }

        {
          NTSCify::SignalDecode::TVKnobSettings info;
          info.saturation = 1.0f;
          info.brightness = 1.0f;
          info.gamma = 1.0f;
          info.tint = 0.0f;
          info.sharpness = 0.0f;

          loadedTexture->sVideoToYIQ->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get(), info);
          #if 0
            loadedTexture->blurIQ->Apply(s_graphicsDevice.get(), loadedTexture->ProcessContext.get());
          #endif

          loadedTexture->yiqToRGB->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get(), info);
          loadedTexture->filterRGB->Apply(s_graphicsDevice.get(), loadedTexture->processContext.get(), info);
        }

#if 1
        {
          NTSCify::CRT::ScreenSettings settings;

          loadedTexture->rgbToCRT->Render(s_graphicsDevice.get(), loadedTexture->processContext.get(), settings);
        }
#endif
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
