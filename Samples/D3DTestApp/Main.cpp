#define NOMINMAX

#include <Windows.h>
#include <CommCtrl.h>

#include "CathodeRetro/SettingPresets.h"

#include "DemoHandler.h"
#include "WicTexture.h"
#include "resource.h"
#include "SettingsDialog.h"


// This is defined by the demo application.
std::unique_ptr<IDemoHandler> MakeDemoHandler(HWND hwnd);


struct LoadedTexture
{
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint32_t> data;

  std::unique_ptr<CathodeRetro::ITexture> oddTexture;
  std::unique_ptr<CathodeRetro::ITexture> evenTexture;
};


static HWND s_hwnd;
static HMENU s_menu;
static HINSTANCE s_instance;
static bool s_fullscreen = false;
static RECT s_oldWindowedRect;

static auto s_signalType = CathodeRetro::SignalType::Composite;
static auto s_sourceSettings = CathodeRetro::k_sourcePresets[1].settings;
static auto s_artifactSettings = CathodeRetro::k_artifactPresets[1].settings;
static auto s_knobSettings = CathodeRetro::TVKnobSettings();
static auto s_overscanSettings = CathodeRetro::OverscanSettings();
static auto s_screenSettings = CathodeRetro::k_screenPresets[4].settings;
static auto s_scanlineType = CathodeRetro::ScanlineType::Odd;


std::unique_ptr<LoadedTexture> s_loadedTexture;
std::unique_ptr<IDemoHandler> s_demoHandler;

void LoadTexture(const wchar_t *path)
{
  bool interlaced = (wcsstr(path, L"Interlaced") != 0 || wcsstr(path, L"interlaced") != 0);

  if (path == nullptr || path[0] == L'\0')
  {
    s_loadedTexture = nullptr;
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

    load->oddTexture = s_demoHandler->CreateRGBATexture(width, height/2, oddScanlines.data());
    load->evenTexture = s_demoHandler->CreateRGBATexture(width, height/2, evenScanlines.data());
  }
  else
  {
    load->oddTexture = s_demoHandler->CreateRGBATexture(width, height, load->data.data());
    load->evenTexture = nullptr;
  }

  s_loadedTexture = std::move(load);

  SendMessage(s_hwnd, WM_SETTINGS_CHANGED, 0, 0);
}


static void OpenFile()
{
  constexpr DWORD k_maxLen = 1024;
  wchar_t filename[k_maxLen] = {};
  OPENFILENAME ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
  ofn.hwndOwner = s_hwnd;
  ofn.lpstrFilter = L"Images (*.jpg;*.jpeg;*.png;*.bmp)\0*.jpg;*.jpeg;*.png;*.bmp\0\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = k_maxLen;
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

  RECT r;
  GetClientRect(s_hwnd, &r);
  s_demoHandler->ResizeBackbuffer(r.right - r.left, r.bottom - r.top);
}


static void Render()
{
  using namespace CathodeRetro;

  if (s_loadedTexture == nullptr)
  {
    return;
  }

  if (s_loadedTexture->evenTexture == nullptr)
  {
    s_scanlineType = ScanlineType::Progressive;
  }
  else if (s_scanlineType == ScanlineType::Odd)
  {
    s_scanlineType = ScanlineType::Even;
  }
  else
  {
    s_scanlineType = ScanlineType::Odd;
  }

  const ITexture *input = (s_scanlineType == ScanlineType::Even && s_loadedTexture->evenTexture != nullptr)
    ? s_loadedTexture->evenTexture.get()
    : s_loadedTexture->oddTexture.get();
  const ITexture *input2 = (s_scanlineType == ScanlineType::Odd && s_loadedTexture->evenTexture != nullptr)
    ? s_loadedTexture->evenTexture.get()
    : s_loadedTexture->oddTexture.get();

  s_demoHandler->Render(input, input2, s_scanlineType);
}


LRESULT FAR PASCAL WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
  switch (message)
  {
  case WM_CREATE:
    break;

  case WM_DESTROY:
    PostQuitMessage( 0 );
    break;

  case WM_SETTINGS_CHANGED:
    if (s_loadedTexture != nullptr)
    {
      s_demoHandler->UpdateCathodeRetroSettings(
        s_signalType,
        s_loadedTexture->oddTexture->Width(),
        s_loadedTexture->oddTexture->Height(),
        s_sourceSettings,
        s_artifactSettings,
        s_knobSettings,
        s_overscanSettings,
        s_screenSettings);
    }

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
      RunSettingsDialog(
        hWnd,
        &s_signalType,
        &s_sourceSettings,
        &s_artifactSettings,
        &s_knobSettings,
        &s_overscanSettings,
        &s_screenSettings);
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
      RunSettingsDialog(
        hWnd,
        &s_signalType,
        &s_sourceSettings,
        &s_artifactSettings,
        &s_knobSettings,
        &s_overscanSettings,
        &s_screenSettings);
      break;
    }

    // Fallthrough to pick up Alt-Enter because some versions of windows only send WM_SYSKEYDOWN for left alt and not right alt.
    // [[fallthrough]];

  case WM_SYSKEYDOWN:
    switch (wParam)
    {
    case VK_RETURN:
      if (HIWORD(lParam) & KF_ALTDOWN)
      {
        ToggleFullscreen();
      }
      break;
    }
    break;

  case WM_TIMER:
    // Stop the timer so WM_TIMER messages don't pile up while we're rolling.
    KillTimer(hWnd, 0);

    // Do the actual rendering
    Render();

    // Now start our timer again with a short delay to ensure it happens again.
    SetTimer(s_hwnd, 0, 3, nullptr);
    break;

  case WM_CLOSE:
    break;

  case WM_SIZE:
    if (wParam != SIZE_MINIMIZED)
    {
      s_demoHandler->ResizeBackbuffer(LOWORD(lParam), HIWORD(lParam));
    }
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
  wc.lpszClassName = L"Cathode Retro D3D Test App";
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
    L"Cathode Retro D3D Test App",
    L"Cathode Retro D3D Test App",
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

    s_demoHandler = MakeDemoHandler(s_hwnd);
    s_demoHandler->UpdateCathodeRetroSettings(
      s_signalType,
      256,
      240,
      s_sourceSettings,
      s_artifactSettings,
      s_knobSettings,
      s_overscanSettings,
      s_screenSettings);

    {
      RECT r;
      GetClientRect(s_hwnd, &r);
      s_demoHandler->ResizeBackbuffer(r.right - r.left, r.bottom - r.top);
    }

    ShowWindow(s_hwnd, SW_NORMAL);
    SetMenu(s_hwnd, s_menu);

    // Set a short timer that will trigger a render
    SetTimer(s_hwnd, 0, 3, nullptr);

    for (;;)
    {
      if(!GetMessage(&msg, nullptr, 0, 0))
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

  return 0;
}
