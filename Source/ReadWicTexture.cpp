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

#include "ReadWicTexture.h"

#define CHECK_HRESULT(exp, opName) \
  do \
  { \
    if (auto res = (exp); FAILED(res)) \
    { \
      char exceptionStr[1024]; \
      sprintf_s(exceptionStr, "Failed to " opName ", result: %08x", uint32_t(res)); \
      throw std::exception(exceptionStr); \
    } \
  } \
  while (false)


SimpleArray<uint32_t> ReadWicTexture(const wchar_t *inputPath, uint32_t *width, uint32_t *height)
{
  ComPtr<IWICImagingFactory> imagingFactory;
  CHECK_HRESULT(
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)imagingFactory.AddressForReplace()),
    "Creating imaging factory");

  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frameDecode;
  ComPtr<IWICFormatConverter> convertedFrame;

  // Create a decoder for the given file

  CHECK_HRESULT(
    imagingFactory->CreateDecoderFromFilename(inputPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.AddressForReplace()),
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
  SimpleArray<uint32_t> colorData(size);
  CHECK_HRESULT(
    convertedFrame->CopyPixels(nullptr, *width * sizeof(uint32_t), size * sizeof(uint32_t), reinterpret_cast<uint8_t*>(colorData.Ptr())),
    "copying pixels");

  return colorData;
}
