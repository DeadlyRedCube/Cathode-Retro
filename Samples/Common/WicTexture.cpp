#pragma warning (push)
#pragma warning (disable: 4987 4623 4626 5027)
#include <stdio.h>
#include <wchar.h>
#pragma warning (pop)

#pragma warning (push)
#pragma warning (disable: 4668 4917 4365 4987 4623 4626 5027)
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma warning (pop)

#include "ComPtr.h"

#include "WicTexture.h"

#define CHECK_HRESULT(exp, opName) \
  do \
  { \
    auto res = (exp); \
    if (FAILED(res)) \
    { \
      char exceptionStr[1024]; \
      sprintf_s(exceptionStr, "Failed to " opName ", result: %08x", uint32_t(res)); \
      throw std::exception(exceptionStr); \
    } \
  } \
  while (false)


void SaveWicTexture(const wchar_t *inputPath, uint32_t width, uint32_t height, const std::vector<uint32_t> &colors)
{
  std::vector<uint32_t> swizzledColors(colors.size());
  for (uint32_t i = 0; i < colors.size(); i++)
  {
    swizzledColors[i] =
      (colors[i] & 0xFF00FF00)
      | ((colors[i] & 0x000000FF) << 16)
      |((colors[i] & 0x00FF0000) >> 16);
  }

  ComPtr<IWICImagingFactory> imagingFactory;
  CHECK_HRESULT(
    CoCreateInstance(
      CLSID_WICImagingFactory,
      nullptr,
      CLSCTX_INPROC_SERVER,
      IID_IWICImagingFactory,
      (void**)imagingFactory.AddressForReplace()),
    "creating imaging factory");

  ComPtr<IWICStream> stream;
  CHECK_HRESULT(
    imagingFactory->CreateStream(stream.AddressForReplace()),
    "creating stream");


  CHECK_HRESULT(
    stream->InitializeFromFilename(inputPath, GENERIC_WRITE),
    "initializing stream");

  ComPtr<IWICBitmapEncoder> encoder;
  CHECK_HRESULT(
    imagingFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.AddressForReplace()),
    "creating encoder");

  CHECK_HRESULT(
    encoder->Initialize(stream,  WICBitmapEncoderNoCache),
    "initializing encoder");

  ComPtr<IWICBitmapFrameEncode> frameEncode;
  CHECK_HRESULT(
    encoder->CreateNewFrame(frameEncode.AddressForReplace(), nullptr),
    "creating new frame");

  CHECK_HRESULT(
    frameEncode->Initialize(nullptr),
    "initializing frame encoder");

  frameEncode->SetSize(width, height);

  auto guid = GUID_WICPixelFormat32bppBGRA;
  CHECK_HRESULT(
    frameEncode->SetPixelFormat(&guid),
    "setting pixel format");

  if (guid != GUID_WICPixelFormat32bppBGRA)
  {
    char exceptionStr[1024];
    sprintf_s(exceptionStr, "Failed to set expected pixel format");
    throw std::exception(exceptionStr);
  }

  CHECK_HRESULT(
    frameEncode->WritePixels(
      height,
      width * sizeof(uint32_t),
      width * height * sizeof(uint32_t),
      const_cast<uint8_t *>(reinterpret_cast<const uint8_t*>(swizzledColors.data()))),
    "writing pixels");

  CHECK_HRESULT(
    frameEncode->Commit(),
    "committing frame");

  CHECK_HRESULT(
    encoder->Commit(),
    "committing encoder");

  frameEncode = nullptr;
  encoder = nullptr;

  CHECK_HRESULT(stream->Commit(STGC_OVERWRITE), "committing stream");
  stream = nullptr;
}


std::vector<uint32_t> ReadWicTexture(const wchar_t *inputPath, uint32_t *width, uint32_t *height)
{
  ComPtr<IWICImagingFactory> imagingFactory;
  CHECK_HRESULT(
    CoCreateInstance(
      CLSID_WICImagingFactory,
      nullptr,
      CLSCTX_INPROC_SERVER,
      IID_IWICImagingFactory,
      (void**)imagingFactory.AddressForReplace()),
    "Creating imaging factory");

  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frameDecode;
  ComPtr<IWICFormatConverter> convertedFrame;

  // Create a decoder for the given file

  CHECK_HRESULT(
    imagingFactory->CreateDecoderFromFilename(
      inputPath,
      nullptr,
      GENERIC_READ,
      WICDecodeMetadataCacheOnDemand,
      decoder.AddressForReplace()),
    "Creating image decoder");

  CHECK_HRESULT(decoder->GetFrame(0, frameDecode.AddressForReplace()), "Getting decoder frame");

  frameDecode->GetSize(width, height);

  CHECK_HRESULT(imagingFactory->CreateFormatConverter(convertedFrame.AddressForReplace()), "creating format converter");
  CHECK_HRESULT(
    convertedFrame->Initialize(
      frameDecode,
      GUID_WICPixelFormat32bppRGBA,
      WICBitmapDitherTypeNone,
      nullptr,
      0.0f,
      WICBitmapPaletteTypeCustom),
    "initializing converted frame");

  uint32_t size = *width * *height;
  std::vector<uint32_t> colorData;
  colorData.resize(size);
  CHECK_HRESULT(
    convertedFrame->CopyPixels(nullptr, *width * sizeof(uint32_t), size * sizeof(uint32_t), reinterpret_cast<uint8_t*>(colorData.data())),
    "copying pixels");

  return colorData;
}
