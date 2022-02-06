#include "WindowsInc.h"

#include <chrono>
#include <cmath>
#include <complex>
#include <vector>
#include <stdint.h>
#include <inttypes.h>

#include <immintrin.h>

#pragma warning (push)
#pragma warning (disable: 4668 4917 4365 4987 4623 4626 5027 4169 4234 4235)
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#pragma warning (pop)

#include <Shlwapi.h>
//#include "resource.h"

#include "BaseTypes.h"
#include "Mem.h"

#include "SP.h"
#include "ReadWicTexture.h"

#include <algorithm>

#include <windowsx.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

#include "NTSCContext.h"
#include "NTSCRGBGenerator.h"
#include "NTSCLowBandpassYCSeparator.h"
#include "NTSCSignalDecoder.h"
#include "SimpleYCSeparator.h"

#define MAX_LOADSTRING 100

HWND window;

using namespace NTSC;


SP<ID3D11Device> device;
SP<IDXGISwapChain> swapChain;
SP<ID3D11DeviceContext> deviceContext;
SP<ID3D11Texture2D> backbuffer;
SP<ID3D11RenderTargetView> backbufferView;
SP<ID3D11RasterizerState> rasterState;
SP<ID3D11DepthStencilState> depthState;
SP<ID3D11BlendState> blendState;
SP<ID3D11InputLayout> inputLayout;
SP<ID3D11VertexShader> vs;
SP<ID3D11PixelShader> ps;
SP<ID3D11Buffer> vb;
SP<ID3D11Buffer> ib;
SP<ID3D11SamplerState> sampler;

int screenWidth;
int screenHeight;
bool fullscreen = true;
f32 aspectAdjust = 1.0f;

bool autoZoom = true;
float zoom = 1.0f;
float panX = 0.5f;
float panY = 0.5f;


const char *pShader = R"(
struct VertexInput
{
  float2 position : POSITION;
  float2 tex0 : TEXCOORD0;
};

struct PixelInput
{
  float4 position : SV_POSITION;
  float2 tex0 : TEXCOORD0;
};

sampler SamDefault : register(s0);
Texture2D<float4> Tex : register(t0);

void VS(VertexInput input, out PixelInput output)
{
  output.position = float4(input.position, 0, 1);
  output.tex0 = input.tex0;
}

float4 PS(PixelInput input) : SV_Target
{
  return Tex.Sample(SamDefault, input.tex0);
}
)";

struct Vertex
{
  f32 positionX;
  f32 positionY;
  f32 texX;
  f32 texY;
};


Vertex vertices[4];
SP<ID3D11Texture2D> displayTexture;
SP<ID3D11ShaderResourceView> displaySRV;
NTSC::GenerationInfo generationInfo = Genesis320WideGenerationInfo; // NESandSNES240pGenerationInfo;


void SetZoom(float zoomIn)
{
  if (displayTexture == nullptr)
  {
    return;
  }
  if(zoomIn == 0)
  {
    panX = panY = 0.5f;
    autoZoom = true;
        
    D3D11_TEXTURE2D_DESC desc;
    displayTexture->GetDesc(&desc);

    // 0 zoom == auto-fit
    f32 panelRatio = f32(screenWidth) / f32(screenHeight);
    f32 imageRatio = f32(desc.Width) / f32(desc.Height) / aspectAdjust;
    s32 drawW, drawH;
    if(panelRatio >= imageRatio)
    {
      drawH = screenHeight;
      drawW = s32(f32(screenHeight)*imageRatio);
    }
    else
    {
      drawW = screenWidth;
      drawH = s32(f32(screenWidth) / imageRatio);
    }        
        
    zoom = f32(drawH) / f32(desc.Height);
  }
  else
  {
    autoZoom = false;
    zoom = zoomIn;
  }
}


struct ProcessContext
{
  NTSC::Context context;
  std::unique_ptr<NTSC::GeneratorBase> generator;
  std::unique_ptr<NTSC::YCSeparatorBase> YCSeparator;
  std::unique_ptr<NTSC::SignalDecoder> decoder;
  AlignedVector<f32> compositeScanline;
  AlignedVector<f32> scanlineScratch;
  AlignedVector<f32> lumaScanline;
  AlignedVector<f32> chromaScanline;
};


void ProcessForNTSC(
  ProcessContext &procCtx,
  const std::vector<u32> &pixelsIn, 
  u32 widthIn, 
  u32 heightIn, 
  std::vector<u32> *pixelsOut, 
  u32 *pWidthOut, 
  u32 *pHeightOut, 
  f32 *pAspectAdjustOut)
{
  pixelsOut->resize(procCtx.context.OutputTexelCount() * heightIn);

  
  *pWidthOut = procCtx.context.OutputTexelCount();
  *pHeightOut = heightIn;
  *pAspectAdjustOut = f32(procCtx.context.GenInfo().outputOversampleAmount) / procCtx.context.GenInfo().pixelAspectRatio;

  //std::chrono::high_resolution_clock c;
  //auto start = c.now();

  procCtx.compositeScanline.resize(Math::AlignInt(procCtx.context.OutputTexelCount(), k_maxFloatAlignment));
  procCtx.scanlineScratch.resize(Math::AlignInt(procCtx.context.OutputTexelCount(), k_maxFloatAlignment));
  procCtx.lumaScanline.resize(Math::AlignInt(procCtx.context.OutputTexelCount(), k_maxFloatAlignment));
  procCtx.chromaScanline.resize(Math::AlignInt(procCtx.context.OutputTexelCount(), k_maxFloatAlignment));

  // for (u32 iteration = 0; iteration < 1000; iteration++)
  {
    procCtx.context.StartFrame();
    for (u32 y = 0; y < heightIn; y++)
    {
      procCtx.generator->ProcessScanlineToComposite(
        pixelsIn.data() + y * widthIn,
        procCtx.context,
        0.0f, // noise scale
        &procCtx.compositeScanline,
        &procCtx.scanlineScratch);

      procCtx.YCSeparator->Separate(
        procCtx.context,
        procCtx.compositeScanline,
        &procCtx.lumaScanline,
        &procCtx.chromaScanline);

      procCtx.decoder->DecodeScanlineToARGB(
        procCtx.context,
        procCtx.lumaScanline,
        procCtx.chromaScanline,
        0.0f, // hue   // !!! 0.2f is approximately the right hue for CGA-from-RGB. Do I need to bake that in?
        1.0f, // saturation
        0.0f, // sharpness
        pixelsOut->data() + y * procCtx.context.OutputTexelCount());
      
      procCtx.context.EndScanline();
    }
  }

  //auto end = c.now();

  //auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  //char buffer[512];
  //sprintf_s(buffer, "%" PRId64 "\n", time);
  //OutputDebugStringA(buffer);
}


struct LoadedTexture {
  u32 width = 0;
  u32 height = 0;
  std::vector<u32> data;
};

LoadedTexture loadedTexture;

void LoadTexture(wchar_t *path)
{
  if (path == nullptr || path[0] == L'\0')
  {
    displayTexture = nullptr;
    displaySRV = nullptr;
    return;
  }
  std::vector<u32> dataFromTex;
  u32 width;
  u32 height;
  ReadWicTexture(path, &dataFromTex, &width, &height);

  loadedTexture.width = width;
  loadedTexture.height = height;
  loadedTexture.data = std::move(dataFromTex);
}


bool dragging = false;
s32 dragX = 0;
s32 dragY = 0;


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_LBUTTONDOWN:
    if (!dragging && displayTexture != nullptr)
    {
      SetCapture(hWnd);
      dragX = GET_X_LPARAM(lParam);
      dragY = GET_Y_LPARAM(lParam);
      SetCursor(nullptr);

      dragging = true;
    }
    break;

  case WM_MOUSEMOVE:
    if (dragging)
    {
      s32 x = GET_X_LPARAM(lParam);
      s32 y = GET_Y_LPARAM(lParam);

      if (x == dragX && y == dragY)
      {
        break;
      }

      D3D11_TEXTURE2D_DESC desc;
      displayTexture->GetDesc(&desc);
      panX -= 3*f32(x - dragX) / zoom / f32(desc.Width) / f32(aspectAdjust);
      panY -= 3*f32(y - dragY) / zoom / f32(desc.Height);
      panX = std::clamp(panX, 0.0f, 1.0f);
      panY = std::clamp(panY, 0.0f, 1.0f);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(window);

      POINT pt;
      pt.x = dragX;
      pt.y = dragY;
      ClientToScreen(hWnd, &pt);
      SetCursorPos(pt.x, pt.y);
    }
    break;

  case WM_LBUTTONUP:
    if (dragging)
    {
      dragging = false;
      SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
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
        ofn.nMaxFile = ArrayLength(filename);
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileName(&ofn))
        {
          LoadTexture(filename);
        }
      }
      break;

    case VK_ADD:
      if (zoom >= 1)
      {
        zoom = f32(s32(zoom + 1));
      }
      else
      {
        zoom = 1;
      }
      SetZoom(zoom);
      break;

    case VK_SUBTRACT:
      if (zoom > 1)
      {
        zoom = f32(s32(zoom - 1));
      }
      SetZoom(zoom);
      break;

    case VK_DIVIDE:
      SetZoom(0);
      break;

    case VK_MULTIPLY:
      SetZoom(1);
      break;

    case VK_ESCAPE:
      DestroyWindow(hWnd);
      break;

    case VK_F11:
      fullscreen = !fullscreen;
      if (fullscreen)
      {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
      }
      else
      {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
      }
      PostMessage(hWnd, WM_SIZE, 0, 0);
      break;
    }
    break;

  case WM_SIZE:
    {
      RECT r;
      GetClientRect(hWnd, &r);
      int width = r.right - r.left;
      int height = r.bottom - r.top;
      if (width == 0 || height == 0)
      {
        break;
      }

      backbuffer = nullptr;
      backbufferView = nullptr;

      screenWidth = width;
      screenHeight = height;

      if (swapChain != nullptr)
      {
        swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        swapChain->GetBuffer(0,  __uuidof((ID3D11Texture2D*)(backbuffer)), reinterpret_cast<void**>(backbuffer.Address()));
        device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.Address());
      }
    }
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}


void CompileShader(const char *shaderText, const char *entryPoint, const char *compileType, std::vector<u8> *outData)
{
  u32 flags = 0; //D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if DEBUG
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

  SP<ID3DBlob> code;
  SP<ID3DBlob> errors;
  HRESULT hr = D3DCompile(
    shaderText,
    strlen(shaderText),
    nullptr,
    nullptr,
    nullptr,
    entryPoint,
    compileType,
    flags,
    0,
    code.Address(),
    errors.Address());

  if (FAILED(hr))
  {
    [[maybe_unused]] const char * except = (const char *)errors->GetBufferPointer();
    ASSERT(false);
  }

  u32 codeSize = u32(code->GetBufferSize());
  u8 * codeBytes = reinterpret_cast<u8*>(code->GetBufferPointer());

  outData->resize(codeSize);
  memcpy(outData->data(), codeBytes, codeSize);
}


bool StringEquals(const wchar_t *a, const wchar_t *b)
{
  return wcscmp(a, b) == 0;
}


int APIENTRY wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] LPWSTR lpCmdLine, int nCmdShow)
{
  CoInitialize(nullptr);

  InitDebugger();

  vertices[0] = {100, 100, 0, 0};
  vertices[1] = {500, 100, 1, 0};
  vertices[2] = {500, 800, 1, 1};
  vertices[3] = {100, 800, 0, 1};

  HMONITOR primaryMon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info;
  ZeroType(&info);
  info.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(primaryMon, &info);

  WNDCLASSEXW wcex;
  ZeroType(&wcex);

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  //wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMPLEIMAGEVIEW2));
  //wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
  //wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLEIMAGEVIEW2);
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = nullptr;
  wcex.lpszClassName = L"NTSCify";

  RegisterClassExW(&wcex);
  HWND hWnd = CreateWindowW(
    L"NTSCify", 
    L"NTSCify", 
    WS_OVERLAPPEDWINDOW, 
    0, 
    0, 
    info.rcMonitor.right - info.rcMonitor.left, 
    info.rcMonitor.bottom - info.rcMonitor.top, 
    nullptr, 
    nullptr, 
    hInstance, 
    nullptr);

  if (!hWnd)
  {
    return FALSE;
  }

  DXGI_SWAP_CHAIN_DESC swapDesc;
  ZeroMemory(&swapDesc, sizeof(swapDesc));
  swapDesc.BufferCount = 2;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.OutputWindow = hWnd;
  swapDesc.Windowed = true;
  swapDesc.SampleDesc.Count = 1;
    
  swapDesc.BufferDesc.RefreshRate.Numerator = 60;
  swapDesc.BufferDesc.RefreshRate.Denominator = 1;

  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  {
    RECT r;
    GetClientRect(hWnd, &r);
    screenWidth = swapDesc.BufferDesc.Width = r.right - r.left;
    screenHeight = swapDesc.BufferDesc.Height = r.bottom - r.top;
  }

  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

  D3D11CreateDeviceAndSwapChain(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
#if DEBUG
    D3D11_CREATE_DEVICE_DEBUG,
#else
    0,
#endif
    &featureLevel,
    1,
    D3D11_SDK_VERSION,
    &swapDesc,
    swapChain.Address(),
    device.Address(),
    nullptr,
    deviceContext.Address());

  swapChain->GetBuffer(0,  __uuidof(backbuffer.Ptr()), reinterpret_cast<void**>(backbuffer.Address()));
  device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.Address());

  {
    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rasterDesc, rasterState.Address());
  }

  {
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = false;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, blendState.Address());
  }

  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ZeroMemory(&dsDesc, sizeof(dsDesc));
    device->CreateDepthStencilState(&dsDesc, depthState.Address());
  }

  {
    D3D11_SAMPLER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Filter = D3D11_FILTER_ANISOTROPIC; // D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    device->CreateSamplerState(&desc, sampler.Address());
  }

  {
    D3D11_BUFFER_DESC vbDesc;
    ZeroMemory(&vbDesc, sizeof(vbDesc));
    vbDesc.ByteWidth = sizeof(Vertex) * 4;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&vbDesc, nullptr, vb.Address());

    D3D11_BUFFER_DESC ibDesc;
    ZeroMemory(&ibDesc, sizeof(ibDesc));
    ibDesc.ByteWidth = sizeof(u16) * 6;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;

    u16 inds[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_SUBRESOURCE_DATA init;
    init.pSysMem = inds;
    init.SysMemPitch = 0;
    init.SysMemSlicePitch = 0;

    device->CreateBuffer(&ibDesc, &init, ib.Address());
  }

  {
    std::vector<u8> data;
    CompileShader(pShader, "VS", "vs_5_0", &data);
    device->CreateVertexShader(data.data(), data.size(), nullptr, vs.Address());

    D3D11_INPUT_ELEMENT_DESC elements[] = 
    {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0}, 
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device->CreateInputLayout(
      elements,
      2,
      data.data(),
      data.size(),
      inputLayout.Address());
  }

  {
    std::vector<u8> data;
    CompileShader(pShader, "PS", "ps_5_0", &data);
    device->CreatePixelShader(data.data(), data.size(), nullptr, ps.Address());
  }

  window = hWnd;
  //LoadTexture(currentFilename);

  ShowWindow(hWnd, nCmdShow);
  InvalidateRect(window, nullptr, true);
  UpdateWindow(hWnd);

  if (fullscreen)
  {
    SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
  }

  u32 texWidth = 0;
  u32 texHeight = 0;
  MSG msg;
  std::vector<u32> dataFromNTSC;

  ProcessContext procCtx;
  procCtx.context = NTSC::Context(generationInfo);
  procCtx.generator = std::make_unique<NTSC::RGBGenerator>();
  procCtx.YCSeparator = std::make_unique<NTSC::SimpleYCSeparator>(procCtx.context.GenInfo());
  procCtx.decoder = std::make_unique<NTSC::SignalDecoder>(procCtx.context.GenInfo(), NTSC::SignalDecoder::FilterType::Simple);
  bool done = false;
  do
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

    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    deviceContext->ClearRenderTargetView(backbufferView, black);

    if (loadedTexture.width != 0 && loadedTexture.height != 0)
    {
      u32 newWidth;
      u32 newHeight;
      ProcessForNTSC(
        procCtx,
        loadedTexture.data, 
        loadedTexture.width, 
        loadedTexture.height, 
        &dataFromNTSC, 
        &newWidth, 
        &newHeight, 
        &aspectAdjust);
      
      if (newWidth != texWidth || newHeight != texHeight)
      {
        texWidth = newWidth;
        texHeight = newHeight;

        D3D11_TEXTURE2D_DESC desc;
        ZeroType(&desc);
        desc.Width = texWidth;
        desc.Height = texHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA resDat;
        ZeroType(&resDat);
        resDat.pSysMem = dataFromNTSC.data();
        resDat.SysMemPitch = texWidth * sizeof(u32);
        device->CreateTexture2D(&desc, &resDat, displayTexture.Address());
        device->CreateShaderResourceView(displayTexture, nullptr, displaySRV.Address());

        SetZoom(autoZoom ? 0.0f : 1.0f);
      }
      else
      {
        D3D11_MAPPED_SUBRESOURCE map;
        deviceContext->Map(displayTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        auto output = static_cast<u32 *>(map.pData);
        for (u32 y = 0; y < texHeight; y++)
        {
          memcpy(output, &dataFromNTSC[y * texWidth], texWidth * sizeof(u32));
          output += map.RowPitch / sizeof(u32);
        }

        deviceContext->Unmap(displayTexture, 0);
      }
    }

    if (displayTexture != nullptr)
    {
      D3D11_TEXTURE2D_DESC desc;
      displayTexture->GetDesc(&desc);

      f32 drawW = f32(desc.Width) * zoom / aspectAdjust;
      f32 drawH = f32(desc.Height) * zoom;
      f32 panXRemainder = std::max(0.0f, drawW - f32(screenWidth));
      f32 panYRemainder = std::max(0.0f, drawH - f32(screenHeight));

      f32 posX = -panX * panXRemainder;
      f32 posY = -panY * panYRemainder;

      if (panXRemainder == 0)
      {
        posX = (f32(screenWidth) - drawW) * 0.5f;
      }
      if (panYRemainder == 0)
      {
        posY = (f32(screenHeight) - drawH) * 0.5f;
      }

      posX /= f32(screenWidth);
      posY /= f32(screenHeight);
      drawW /= f32(screenWidth);
      drawH /= f32(screenHeight);

      posX = posX*2-1;
      posY = -posY*2+1;
      drawW = drawW*2;
      drawH = -drawH*2;

      D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD;
      D3D11_MAPPED_SUBRESOURCE subres;
      deviceContext->Map(vb, 0, mapType, 0, &subres);
      auto vbData = reinterpret_cast<Vertex*>(subres.pData);
      vbData[0] = {posX,         posY,         0, 0};
      vbData[1] = {posX + drawW, posY,         1, 0};
      vbData[2] = {posX + drawW, posY + drawH, 1, 1};
      vbData[3] = {posX,         posY + drawH, 0, 1};
      deviceContext->Unmap(vb, 0);

      D3D11_VIEWPORT vp;
      vp.TopLeftX = 0;
      vp.TopLeftY = 0;
      vp.Width = float(screenWidth);
      vp.Height = float(screenHeight);
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;
      deviceContext->OMSetRenderTargets(1, backbufferView.ConstAddress(), nullptr);
      deviceContext->RSSetViewports(1, &vp);

      u32 stride = sizeof(Vertex);
      u32 offsets = 0;
      deviceContext->IASetVertexBuffers(0, 1, vb.ConstAddress(), &stride, &offsets);
      deviceContext->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      deviceContext->OMSetBlendState(blendState, black, 0xffffffff);
      deviceContext->OMSetDepthStencilState(depthState, 0);
      deviceContext->RSSetState(rasterState);
      deviceContext->VSSetShader(vs, nullptr, 0);
      deviceContext->PSSetShader(ps, nullptr, 0);
      deviceContext->IASetInputLayout(inputLayout);

      deviceContext->PSSetSamplers(0, 1, sampler.ConstAddress());
      deviceContext->PSSetShaderResources(0, 1, displaySRV.ConstAddress());

      deviceContext->DrawIndexed(6, 0, 0);
    }
    swapChain->Present(1, 0);
  }
  while (!done);

  return int(msg.wParam);
}
