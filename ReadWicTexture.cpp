#include "Common.h"
#include "BaseTypes.h"
#include "Debug.h"
#include "WindowsInc.h"

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

#include "SP.h"

#include "ReadWicTexture.h"

void ReadWicTexture(const wchar_t *inputPath, std::vector<u32> *colorData, u32 *width, u32 *height)
{
    SP<IWICImagingFactory> imagingFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)imagingFactory.Address());
    ASSERT(SUCCEEDED(hr));

    SP<IWICBitmapDecoder> decoder;
    SP<IWICBitmapFrameDecode> frameDecode;
    SP<IWICFormatConverter> convertedFrame;

    // Create a decoder for the given file

    hr = imagingFactory->CreateDecoderFromFilename(inputPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.Address());
    ASSERT(SUCCEEDED(hr));

    hr = decoder->GetFrame(0, frameDecode.Address());
    ASSERT(SUCCEEDED(hr));

    frameDecode->GetSize(width, height);

    hr = imagingFactory->CreateFormatConverter(convertedFrame.Address());
    ASSERT(SUCCEEDED(hr));
    hr = convertedFrame->Initialize(
        frameDecode,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    ASSERT(SUCCEEDED(hr));

    u32 size = *width * *height;
    colorData->resize(size);
    hr = convertedFrame->CopyPixels(nullptr, *width * sizeof(u32), size * sizeof(u32), reinterpret_cast<u8*>(colorData->data()));
    ASSERT(SUCCEEDED(hr));
}