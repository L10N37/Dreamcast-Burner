#include "texture_loader.h"

#include <windows.h>
#include <wincodec.h>

#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

Texture LoadPngResource(ID3D11Device* device, const int resourceId) {
    Texture result;
    if (device == nullptr) {
        return result;
    }

    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (resource == nullptr) {
        return result;
    }

    const HGLOBAL loaded = LoadResource(module, resource);
    const DWORD byteCount = SizeofResource(module, resource);
    auto* bytes = static_cast<std::uint8_t*>(LockResource(loaded));
    if (loaded == nullptr || bytes == nullptr || byteCount == 0) {
        return result;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
    }
    if (FAILED(hr)) {
        return result;
    }

    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(bytes, byteCount))) {
        return result;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(
            stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
        return result;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        return result;
    }

    UINT width = 0;
    UINT height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) {
        return result;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom))) {
        return result;
    }

    const UINT stride = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(
            nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) {
        return result;
    }

    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = pixels.data();
    initialData.SysMemPitch = stride;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&description, &initialData, &texture)) ||
        FAILED(device->CreateShaderResourceView(
            texture.Get(), nullptr, &result.view))) {
        result.view.Reset();
        return result;
    }

    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    return result;
}

