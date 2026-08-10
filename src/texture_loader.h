#pragma once

#include <d3d11.h>
#include <wrl/client.h>

struct Texture final {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool IsValid() const noexcept { return view != nullptr; }
};

[[nodiscard]] Texture LoadPngResource(ID3D11Device* device, int resourceId);

