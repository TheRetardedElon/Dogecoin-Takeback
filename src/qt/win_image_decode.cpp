// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "win_image_decode.h"

#ifndef WIN32
QImage DecodeImageBytesWin(const QByteArray& bytes)
{
    Q_UNUSED(bytes);
    return QImage();
}
#else

#include <windows.h>
#include <wincodec.h>
#include <objbase.h>

#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

QImage DecodeImageBytesWin(const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return QImage();

    HRESULT hrCo = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hrCo) || hrCo == S_FALSE;

    IWICImagingFactory* factory = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)bytes.size());
    if (!hMem) {
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }
    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }
    memcpy(pMem, bytes.constData(), (size_t)bytes.size());
    GlobalUnlock(hMem);

    IStream* stream = NULL;
    hr = CreateStreamOnHGlobal(hMem, TRUE /* free on release */, &stream);
    if (FAILED(hr) || !stream) {
        GlobalFree(hMem);
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    IWICBitmapDecoder* decoder = NULL;
    hr = factory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        stream->Release();
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    IWICBitmapFrameDecode* frame = NULL;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        decoder->Release();
        stream->Release();
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    IWICFormatConverter* converter = NULL;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                               NULL, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (needUninit)
            CoUninitialize();
        return QImage();
    }

    const UINT stride = width * 4;
    const UINT bufSize = stride * height;
    std::vector<unsigned char> buf(bufSize);
    hr = converter->CopyPixels(NULL, stride, bufSize, buf.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    if (needUninit)
        CoUninitialize();

    if (FAILED(hr))
        return QImage();

    // QImage owns a copy
    QImage img(buf.data(), (int)width, (int)height, (int)stride, QImage::Format_ARGB32_Premultiplied);
    return img.copy();
}

#endif // WIN32
