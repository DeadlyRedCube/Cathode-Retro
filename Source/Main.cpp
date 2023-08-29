#define NOMINMAX
#pragma once
#include <Windows.h>
#include <CommCtrl.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "NTSCify/Internal/CRT/RGBToCRT.h"
#include "NTSCify/Internal/Generator/SignalGenerator.h"
#include "NTSCify/Internal/Decoder/SignalDecoder.h"
#include "D3D11GraphicsDevice.h"
#include "WicTexture.h"
#include "resource.h"
#include "SettingsDialog.h"

static HWND s_hwnd;
static HMENU s_menu;
static HINSTANCE s_instance;
static bool s_fullscreen = false;
static RECT s_oldWindowedRect;
static std::unique_ptr<ID3D11GraphicsDevice> s_graphicsDevice;

static auto s_signalType = NTSCify::SignalType::Composite;
static auto s_sourceSettings = NTSCify::k_sourcePresets[1].settings;
static auto s_artifactSettings = NTSCify::k_artifactPresets[1].settings;
static auto s_knobSettings = NTSCify::TVKnobSettings();
static auto s_overscanSettings = NTSCify::OverscanSettings();
static auto s_screenSettings = NTSCify::k_screenPresets[2].settings;

static std::thread s_renderThread;
static std::atomic<bool> s_stopRenderThread = false;
static std::mutex s_renderThreadMutex;

struct LoadedTexture 
{
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint32_t> data;

  std::unique_ptr<ITexture> oddTexture;
  std::unique_ptr<ITexture> evenTexture;

  std::unique_ptr<NTSCify::Internal::Generator::SignalGenerator> signalGenerator;
  std::unique_ptr<NTSCify::Internal::Decoder::SignalDecoder> signalDecoder;
  std::unique_ptr<NTSCify::Internal::CRT::RGBToCRT> rgbToCRT;

  NTSCify::SignalType cachedSignalType {};
  NTSCify::SourceSettings cachedSourceSettings {};
};

std::unique_ptr<LoadedTexture> loadedTexture;

enum class Rebuild
{
  AsNeeded,
  Always,
  Never,
};


void RebuildGeneratorsIfNecessary(Rebuild rebuild)
{ 
  using namespace NTSCify::Internal;

  if (loadedTexture == nullptr)
  {
    return;
  }

  auto sourceSettings = s_sourceSettings;
  auto signalType = s_signalType;

  if (rebuild == Rebuild::Never)
  {
    loadedTexture->cachedSignalType = signalType;
    loadedTexture->cachedSourceSettings = sourceSettings;
  }
  else if (rebuild == Rebuild::Always
    || signalType != loadedTexture->cachedSignalType
    || memcmp(&sourceSettings, &loadedTexture->cachedSourceSettings, sizeof(sourceSettings)) != 0)
  {
    loadedTexture->signalGenerator = std::make_unique<Generator::SignalGenerator>(
      s_graphicsDevice.get(), 
      s_signalType,
      loadedTexture->oddTexture->Width(),
      loadedTexture->oddTexture->Height(),
      s_sourceSettings);
    loadedTexture->signalDecoder = std::make_unique<Decoder::SignalDecoder>(
      s_graphicsDevice.get(), 
      loadedTexture->signalGenerator->SignalProperties());
    loadedTexture->rgbToCRT = std::make_unique<CRT::RGBToCRT>(
      s_graphicsDevice.get(), 
      loadedTexture->oddTexture->Width(), 
      loadedTexture->signalGenerator->SignalProperties().scanlineWidth, 
      loadedTexture->oddTexture->Height(),
      loadedTexture->signalGenerator->SignalProperties().inputPixelAspectRatio);

    loadedTexture->cachedSignalType = signalType;
    loadedTexture->cachedSourceSettings = sourceSettings;
  }
}


void LoadTexture(const wchar_t *path, Rebuild rebuild = Rebuild::Always)
{
  bool interlaced = (wcsstr(path, L"Interlaced") != 0 || wcsstr(path, L"interlaced") != 0);
  std::unique_lock lock(s_renderThreadMutex);

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

  if (interlaced)
  {
    // Grab the odd 
    std::vector<uint32_t> oddScanlines(width * height/2);
    std::vector<uint32_t> evenScanlines(width * height/2);
    for (uint32_t y = 0; y < height/2; y++)
    {
      memcpy(oddScanlines.data()  + width * y, load->data.data() + width * (y * 2),     width * sizeof(uint32_t));
      memcpy(evenScanlines.data() + width * y, load->data.data() + width * (y * 2 + 1), width * sizeof(uint32_t));
    }

    load->oddTexture = s_graphicsDevice->CreateTexture(width, height/2, 1, TextureFormat::RGBA_Unorm8, TextureFlags::None, oddScanlines.data(), width * sizeof(uint32_t));
    load->evenTexture = s_graphicsDevice->CreateTexture(width, height/2, 1, TextureFormat::RGBA_Unorm8, TextureFlags::None, evenScanlines.data(), width * sizeof(uint32_t));
  }
  else
  {
    load->oddTexture = s_graphicsDevice->CreateTexture(width, height, 1, TextureFormat::RGBA_Unorm8, TextureFlags::None, load->data.data(), width * sizeof(uint32_t));
    load->evenTexture = nullptr;
  }

  loadedTexture = std::move(load);

  if (rebuild != Rebuild::Never)
  {
    RebuildGeneratorsIfNecessary(rebuild);
  }
}




static void OpenFile()
{
  wchar_t filename[1024] = {0};
  OPENFILENAME ofn;
  ZeroType(&ofn);
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
  ofn.hwndOwner = s_hwnd;
  ofn.lpstrFilter = L"Images (*.jpg;*.jpeg;*.png;*.bmp)\0*.jpg;*.jpeg;*.png;*.bmp\0\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = DWORD(k_arrayLength<decltype(filename)>);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

  if (GetOpenFileName(&ofn))
  {
    LoadTexture(filename);
  }
}


void ToggleFullscreen()
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
    SetMenu(s_hwnd, s_menu);
    SetWindowPos(
      s_hwnd, 
      nullptr, 
      s_oldWindowedRect.left, 
      s_oldWindowedRect.top, 
      s_oldWindowedRect.right - s_oldWindowedRect.left,
      s_oldWindowedRect.bottom - s_oldWindowedRect.top,
      SWP_NOZORDER);
  }

  {
    std::unique_lock lock(s_renderThreadMutex);
    s_graphicsDevice->UpdateWindowSize();
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

  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case ID_FILE_OPEN:
      OpenFile();
      break;

    case ID_FILE_EXIT:
      SendMessage(hWnd, WM_CLOSE, 0, 0);
      break;

    case ID_OPTIONS_FULLSCREEN:
      ToggleFullscreen();
      break;
    
    case ID_OPTIONS_SETTINGS:
      RunSettingsDialog(hWnd, &s_signalType, &s_sourceSettings, &s_artifactSettings, &s_knobSettings, &s_overscanSettings, &s_screenSettings);
      break;
    }
    break;

  case WM_KEYDOWN:
    switch (wParam)
    {
    case 'O':
      OpenFile();
      break;

    case 'S':
      RunSettingsDialog(hWnd, &s_signalType, &s_sourceSettings, &s_artifactSettings, &s_knobSettings, &s_overscanSettings, &s_screenSettings);
      break;
      
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

  s_instance = hInstance;

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

  s_menu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAIN_MENU));
}


void RenderLoadedTexture(ITexture *output, Rebuild rebuild = Rebuild::AsNeeded)
{
  using namespace NTSCify::Internal;

  if (rebuild != Rebuild::Never)
  {
    RebuildGeneratorsIfNecessary(rebuild);
  }

  static auto scanlineType = CRT::ScanlineType::Odd;

  
  if (loadedTexture->evenTexture == nullptr)
  {
    scanlineType = CRT::ScanlineType::FauxProgressive;
  }
  else if (scanlineType == CRT::ScanlineType::Odd)
  { 
    scanlineType = CRT::ScanlineType::Even;
  }
  else
  {
    scanlineType = CRT::ScanlineType::Odd;
  }

  const ITexture *input = (scanlineType == CRT::ScanlineType::Even && loadedTexture->evenTexture != nullptr) 
    ? loadedTexture->evenTexture.get() 
    : loadedTexture->oddTexture.get();
  const ITexture *input2 = (scanlineType == CRT::ScanlineType::Odd && loadedTexture->evenTexture != nullptr) 
    ? loadedTexture->evenTexture.get() 
    : loadedTexture->oddTexture.get();
  if (s_signalType != NTSCify::SignalType::RGB)
  {
    loadedTexture->signalGenerator->SetArtifactSettings(s_artifactSettings);
    loadedTexture->signalGenerator->Generate(input);

    loadedTexture->signalDecoder->SetKnobSettings(s_knobSettings);
    loadedTexture->signalDecoder->Decode(
      loadedTexture->signalGenerator->SignalTexture(), 
      loadedTexture->signalGenerator->PhasesTexture(), 
      loadedTexture->signalGenerator->SignalLevels());

    input = loadedTexture->signalDecoder->CurrentFrameRGBOutput();
    input2 = loadedTexture->signalDecoder->PreviousFrameRGBOutput();
  }

  // $TODO Currently artifacts are not applied to RGB, and we might still want to at least have some noise in there?
  loadedTexture->rgbToCRT->SetScreenSettings(s_screenSettings);
  loadedTexture->rgbToCRT->Render(input, input2, output, scanlineType);
}

void RenderThreadProc()
{
  while (!s_stopRenderThread)
  {
    {
      std::unique_lock lock(s_renderThreadMutex);

      s_graphicsDevice->UpdateWindowSize();
      s_graphicsDevice->ClearBackbuffer();

      if (loadedTexture != nullptr)
      {
        RenderLoadedTexture(nullptr);
      }

      s_graphicsDevice->Present();
    }

    Sleep(1);
  }
}



// Do some absolutely not-at-all legit threading to put the drawing in the background so the settings dialog can change things on the fly. Please don't actually write
//  multithreaded code with as few guards on data transfer as I have here.
void StopRenderThread()
{
  if (s_renderThread.joinable())
  {
    s_stopRenderThread = true;
    s_renderThread.join();
  }

  s_stopRenderThread = false;
}


void StartRenderThread()
{
  StopRenderThread();

  s_renderThread = std::thread(RenderThreadProc);
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

    s_graphicsDevice = ID3D11GraphicsDevice::Create(s_hwnd);

    ShowWindow(s_hwnd, SW_NORMAL);
    SetMenu(s_hwnd, s_menu);

    StartRenderThread();
    for (;;)
    {
      if(!GetMessage(&msg, NULL, 0, 0))
      {
        break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  catch (const std::exception &ex)
  {
    MessageBoxA(s_hwnd, ex.what(), "Error", MB_OK);
  }

  StopRenderThread();
  return 0;
}
