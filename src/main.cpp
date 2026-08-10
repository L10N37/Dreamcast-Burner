#include <windows.h>
#include <commdlg.h>
#include <dbt.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "burn_engine.h"
#include "drive_manager.h"
#include "resource.h"
#include "texture_loader.h"

using Microsoft::WRL::ComPtr;

namespace {

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_deviceContext;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_renderTarget;
bool g_driveRefreshRequested = false;
bool g_jobInProgress = false;
std::wstring g_droppedPath;

struct AppState final {
    std::wstring selectedCdi;
    std::vector<OpticalDrive> drives;
    int selectedDrive = 0;
    int selectedSpeed = 0; // 0 means Automatic.
    std::string status = "Choose a CDI image and insert a blank CD-R.";
    BurnStage lastBurnStage = BurnStage::Idle;
};

[[nodiscard]] std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(byteCount), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), byteCount, nullptr, nullptr);
    return result;
}

[[nodiscard]] bool IsCdiPath(const std::wstring& path) {
    const std::size_t dot = path.find_last_of(L'.');
    return dot != std::wstring::npos &&
        _wcsicmp(path.c_str() + dot, L".cdi") == 0;
}

[[nodiscard]] bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void SelectCdi(AppState& state, BurnEngine& burnEngine, std::wstring path) {
    if (!IsCdiPath(path)) {
        state.status = "That file is not a .cdi image.";
        return;
    }
    if (!FileExists(path)) {
        state.status = "Windows cannot find the selected CDI image.";
        return;
    }
    if (path.size() >= MAX_PATH) {
        state.status =
            "CDIrip requires a CDI path shorter than 260 characters. Move the image to a shorter path and retry.";
        return;
    }
    state.selectedCdi = std::move(path);
    burnEngine.Reset();
    state.status = "CDI selected. Check it, or choose Burn Disc when a blank CD-R is ready.";
}

void ShowCdiPicker(AppState& state, BurnEngine& burnEngine) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog)))) {
        state.status = "Windows could not open the file picker.";
        return;
    }

    const COMDLG_FILTERSPEC filters[] = {
        {L"Dreamcast CDI images", L"*.cdi"},
        {L"All files", L"*.*"},
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetTitle(L"Choose a Dreamcast CDI image");

    FILEOPENDIALOGOPTIONS options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    }

    const HRESULT showResult = dialog->Show(nullptr);
    if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }
    if (FAILED(showResult)) {
        state.status = "The CDI picker closed with an error.";
        return;
    }

    ComPtr<IShellItem> selected;
    if (FAILED(dialog->GetResult(&selected))) {
        state.status = "Windows did not return the selected file.";
        return;
    }

    PWSTR path = nullptr;
    if (SUCCEEDED(selected->GetDisplayName(SIGDN_FILESYSPATH, &path)) &&
        path != nullptr) {
        SelectCdi(state, burnEngine, path);
        CoTaskMemFree(path);
    }
}

void RefreshDrives(AppState& state) {
    std::wstring previousRoot;
    if (state.selectedDrive >= 0 &&
        state.selectedDrive < static_cast<int>(state.drives.size())) {
        previousRoot = state.drives[static_cast<std::size_t>(state.selectedDrive)].rootPath;
    }

    state.drives = EnumerateOpticalDrives();
    state.selectedDrive = 0;
    state.selectedSpeed = 0;
    if (!previousRoot.empty()) {
        const auto match = std::find_if(
            state.drives.begin(),
            state.drives.end(),
            [&previousRoot](const OpticalDrive& drive) {
                return drive.rootPath == previousRoot;
            });
        if (match != state.drives.end()) {
            state.selectedDrive = static_cast<int>(
                std::distance(state.drives.begin(), match));
        }
    }

    if (state.drives.empty()) {
        state.status = "No optical burners detected. Connect a drive and press Refresh.";
    } else if (state.drives.size() == 1) {
        state.status = "Detected 1 optical drive.";
    } else {
        state.status = "Detected " + std::to_string(state.drives.size()) +
            " optical drives.";
    }
}

[[nodiscard]] std::string FormatSpeed(const WriteSpeed& speed) {
    const long roundedMultiplier = std::lround(speed.cdMultiplier);
    std::string multiplier;
    if (std::abs(speed.cdMultiplier - static_cast<float>(roundedMultiplier)) < 0.08F) {
        multiplier = std::to_string(roundedMultiplier) + "x";
    } else {
        char buffer[32]{};
        snprintf(buffer, sizeof(buffer), "%.1fx", speed.cdMultiplier);
        multiplier = buffer;
    }

    return multiplier + "  (" + std::to_string(speed.kilobytesPerSecond) +
        " KB/s)";
}

void DrawRotatedImage(
    const Texture& texture,
    const ImVec2 center,
    const float size,
    const float angleRadians) {
    if (!texture.IsValid()) {
        return;
    }

    const float half = size * 0.5F;
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    const auto rotate = [center, cosine, sine](const float x, const float y) {
        return ImVec2(
            center.x + x * cosine - y * sine,
            center.y + x * sine + y * cosine);
    };

    const ImVec2 topLeft = rotate(-half, -half);
    const ImVec2 topRight = rotate(half, -half);
    const ImVec2 bottomRight = rotate(half, half);
    const ImVec2 bottomLeft = rotate(-half, half);
    ImGui::GetWindowDrawList()->AddImageQuad(
        reinterpret_cast<ImTextureID>(texture.view.Get()),
        topLeft,
        topRight,
        bottomRight,
        bottomLeft,
        ImVec2(0.0F, 0.0F),
        ImVec2(1.0F, 0.0F),
        ImVec2(1.0F, 1.0F),
        ImVec2(0.0F, 1.0F));
}

void ConfigureImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(24.0F, 20.0F);
    style.FramePadding = ImVec2(12.0F, 9.0F);
    style.ItemSpacing = ImVec2(10.0F, 10.0F);
    style.ItemInnerSpacing = ImVec2(8.0F, 6.0F);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 5.0F;
    style.FrameRounding = 4.0F;
    style.PopupRounding = 4.0F;
    style.ScrollbarRounding = 4.0F;
    style.GrabRounding = 3.0F;
    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00F, 1.00F, 1.00F, 1.00F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.52F, 0.52F, 0.52F, 1.00F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.00F, 0.00F, 0.00F, 1.00F);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00F, 0.00F, 0.00F, 1.00F);
    colors[ImGuiCol_PopupBg] = ImVec4(0.035F, 0.035F, 0.035F, 0.98F);
    colors[ImGuiCol_Border] = ImVec4(0.28F, 0.28F, 0.28F, 1.00F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.055F, 0.055F, 0.055F, 1.00F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.11F, 0.11F, 0.11F, 1.00F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.17F, 0.17F, 0.17F, 1.00F);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00F, 0.00F, 0.00F, 1.00F);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00F, 0.00F, 0.00F, 1.00F);
    colors[ImGuiCol_Button] = ImVec4(0.18F, 0.055F, 0.01F, 1.00F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.90F, 0.22F, 0.01F, 1.00F);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00F, 0.36F, 0.02F, 1.00F);
    colors[ImGuiCol_Header] = ImVec4(0.18F, 0.055F, 0.01F, 1.00F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.75F, 0.16F, 0.01F, 1.00F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.95F, 0.26F, 0.01F, 1.00F);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00F, 0.32F, 0.02F, 1.00F);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00F, 0.25F, 0.01F, 1.00F);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00F, 0.43F, 0.04F, 1.00F);
    colors[ImGuiCol_Separator] = ImVec4(0.72F, 0.14F, 0.01F, 1.00F);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.94F, 0.20F, 0.01F, 1.00F);
}

void DrawDriveDetails(const OpticalDrive& drive) {
    ImGui::TextDisabled("Firmware");
    ImGui::SameLine(110.0F);
    ImGui::TextUnformatted(drive.firmware.empty() ? "Not reported" : drive.firmware.c_str());
    ImGui::TextDisabled("Connection");
    ImGui::SameLine(110.0F);
    ImGui::TextUnformatted(drive.bus.empty() ? "Unknown" : drive.bus.c_str());
    ImGui::TextDisabled("Media");
    ImGui::SameLine(110.0F);
    ImVec4 mediaColor(1.00F, 0.62F, 0.24F, 1.00F);
    if (drive.mediaPresent && drive.blankMediaKnown && drive.blankMedia &&
        (drive.currentProfile == 0 || drive.currentProfile == 0x0009)) {
        mediaColor = ImVec4(0.45F, 0.95F, 0.50F, 1.00F);
    } else if (drive.mediaPresent && drive.blankMediaKnown && !drive.blankMedia) {
        mediaColor = ImVec4(1.00F, 0.32F, 0.24F, 1.00F);
    }
    ImGui::TextColored(
        mediaColor,
        "%s",
        drive.mediaDescription.empty()
            ? (drive.mediaPresent ? "Disc present" : "No disc / not ready")
            : drive.mediaDescription.c_str());
}

[[nodiscard]] int SelectedSpeedX(
    const AppState& state,
    const OpticalDrive* drive) {
    if (drive == nullptr || state.selectedSpeed <= 0 ||
        state.selectedSpeed > static_cast<int>(drive->writeSpeeds.size())) {
        return 0;
    }
    return std::max(
        1,
        static_cast<int>(std::lround(
            drive->writeSpeeds[static_cast<std::size_t>(state.selectedSpeed - 1)]
                .cdMultiplier)));
}

void DrawApp(
    AppState& state,
    BurnEngine& burnEngine,
    const Texture& artwork,
    const Texture& disc) {
    const ImGuiIO& io = ImGui::GetIO();
    const BurnSnapshot burn = burnEngine.Snapshot();
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(io.DisplaySize);
    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Dreamcast Burner", nullptr, windowFlags);

    ImGui::SetWindowFontScale(1.55F);
    ImGui::TextUnformatted("DREAMCAST BURNER");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::SameLine();
    ImGui::TextDisabled("  native x64  |  CDI to CD-R");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable(
            "MainLayout",
            2,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("Artwork", ImGuiTableColumnFlags_WidthFixed, 390.0F);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();

        const float artSize = std::min(372.0F, ImGui::GetContentRegionAvail().x);
        if (artwork.IsValid()) {
            ImGui::Image(
                reinterpret_cast<ImTextureID>(artwork.view.Get()),
                ImVec2(artSize, artSize));
        } else {
            ImGui::Dummy(ImVec2(artSize, artSize));
            ImGui::TextDisabled("Artwork could not be loaded.");
        }

        constexpr float discSize = 142.0F;
        const float discStart = ImGui::GetCursorPosX() +
            (artSize - discSize) * 0.5F;
        ImGui::SetCursorPosX(discStart);
        const ImVec2 discTopLeft = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(discSize, discSize));
        static float discAngle = 0.0F;
        if (burn.writing) {
            discAngle = std::fmod(discAngle + io.DeltaTime * 3.4F, 6.283185307F);
        }
        DrawRotatedImage(
            disc,
            ImVec2(
                discTopLeft.x + discSize * 0.5F,
                discTopLeft.y + discSize * 0.5F),
            discSize,
            discAngle);

        if (burn.writing || burn.stage == BurnStage::Complete ||
            (burn.stage == BurnStage::Failed && burn.progress > 0.0F)) {
            const int percent = static_cast<int>(std::lround(burn.progress * 100.0F));
            char progressLabel[32]{};
            snprintf(progressLabel, sizeof(progressLabel), "%d%%", percent);
            ImGui::SetNextItemWidth(artSize);
            ImGui::ProgressBar(
                std::clamp(burn.progress, 0.0F, 1.0F),
                ImVec2(artSize, 23.0F),
                progressLabel);

            std::string burnDetails;
            if (burn.stage == BurnStage::Complete) {
                burnDetails = "Complete";
            } else if (burn.writing) {
                burnDetails = "Session " + std::to_string(burn.session) + " of 2";
                if (!burn.actualSpeed.empty()) {
                    burnDetails += "  |  " + burn.actualSpeed;
                }
                if (burn.bufferPercent >= 0) {
                    burnDetails += "  |  buffer " +
                        std::to_string(burn.bufferPercent) + "%";
                }
            } else {
                burnDetails = "Burn failed";
            }
            const float detailWidth = ImGui::CalcTextSize(burnDetails.c_str()).x;
            ImGui::SetCursorPosX(
                ImGui::GetCursorPosX() + std::max(0.0F, (artSize - detailWidth) * 0.5F));
            ImGui::TextUnformatted(burnDetails.c_str());
        }

        ImGui::TableNextColumn();
        ImGui::PushID("Controls");

        ImGui::TextDisabled("DISC IMAGE");
        ImGui::BeginChild("CdiPath", ImVec2(0.0F, 58.0F), ImGuiChildFlags_Borders);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0F);
        if (state.selectedCdi.empty()) {
            ImGui::TextDisabled("Drop a .cdi here or choose Browse");
        } else {
            const std::string path = WideToUtf8(state.selectedCdi);
            ImGui::TextWrapped("%s", path.c_str());
        }
        ImGui::EndChild();

        ImGui::BeginDisabled(burn.busy);
        if (ImGui::Button("Browse...", ImVec2(128.0F, 0.0F))) {
            ShowCdiPicker(state, burnEngine);
        }
        ImGui::SameLine();
        const bool canCheck = !state.selectedCdi.empty();
        ImGui::BeginDisabled(!canCheck);
        if (ImGui::Button("Check CDI", ImVec2(128.0F, 0.0F))) {
            BurnRequest request;
            request.cdiPath = state.selectedCdi;
            request.checkOnly = true;
            (void)burnEngine.Start(std::move(request));
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::TextDisabled("OPTICAL BURNER");
        const std::string drivePreview = state.drives.empty()
            ? "No optical burners found"
            : state.drives[static_cast<std::size_t>(state.selectedDrive)]
                  .DisplayName();
        ImGui::BeginDisabled(burn.busy);
        ImGui::SetNextItemWidth(-92.0F);
        if (ImGui::BeginCombo("##Burner", drivePreview.c_str())) {
            for (int index = 0; index < static_cast<int>(state.drives.size()); ++index) {
                const bool selected = index == state.selectedDrive;
                const std::string name = state.drives[static_cast<std::size_t>(index)]
                                             .DisplayName();
                if (ImGui::Selectable(name.c_str(), selected)) {
                    state.selectedDrive = index;
                    state.selectedSpeed = 0;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(82.0F, 0.0F))) {
            RefreshDrives(state);
        }
        ImGui::EndDisabled();

        const OpticalDrive* drive = state.drives.empty()
            ? nullptr
            : &state.drives[static_cast<std::size_t>(state.selectedDrive)];
        if (drive != nullptr) {
            DrawDriveDetails(*drive);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("WRITE SPEED");
        std::string speedPreview = "Automatic (recommended)";
        if (drive != nullptr && state.selectedSpeed > 0 &&
            state.selectedSpeed <= static_cast<int>(drive->writeSpeeds.size())) {
            speedPreview = FormatSpeed(
                drive->writeSpeeds[static_cast<std::size_t>(state.selectedSpeed - 1)]);
        }
        ImGui::BeginDisabled(burn.busy);
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##WriteSpeed", speedPreview.c_str())) {
            if (ImGui::Selectable("Automatic (recommended)", state.selectedSpeed == 0)) {
                state.selectedSpeed = 0;
            }
            if (drive != nullptr) {
                for (int index = 0;
                     index < static_cast<int>(drive->writeSpeeds.size());
                     ++index) {
                    const std::string label = FormatSpeed(
                        drive->writeSpeeds[static_cast<std::size_t>(index)]);
                    const bool selected = state.selectedSpeed == index + 1;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        state.selectedSpeed = index + 1;
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (drive != nullptr) {
            ImGui::TextDisabled("%s", drive->speedQueryMessage.c_str());
        } else {
            ImGui::TextDisabled("Connect a USB or internal CD writer, then press Refresh.");
        }

        ImGui::Spacing();
        const std::string& currentStatus =
            burn.stage == BurnStage::Idle ? state.status : burn.status;
        ImGui::TextWrapped("%s", currentStatus.c_str());

        const bool profileSupported = drive == nullptr ||
            drive->currentProfile == 0 || drive->currentProfile == 0x0009;
        const bool blankSupported = drive == nullptr ||
            !drive->blankMediaKnown || drive->blankMedia;
        const bool writerSupported = drive == nullptr ||
            !drive->cdWriteCapabilityKnown || drive->canWriteCdR;
        const bool backendReady = drive != nullptr && !drive->cdrecordDevice.empty();
        const bool canBurn = !burn.busy && !state.selectedCdi.empty() &&
            drive != nullptr && drive->mediaPresent && profileSupported &&
            blankSupported && writerSupported && backendReady;
        ImGui::BeginDisabled(!canBurn);
        if (ImGui::Button("BURN DISC", ImVec2(-1.0F, 48.0F))) {
            ImGui::OpenPopup("Confirm burn");
        }
        ImGui::EndDisabled();

        if (!canBurn && !burn.busy) {
            if (state.selectedCdi.empty()) {
                ImGui::TextDisabled("Choose a CDI image first.");
            } else if (drive == nullptr) {
                ImGui::TextDisabled("Connect and select a CD writer.");
            } else if (!drive->mediaPresent) {
                ImGui::TextDisabled("Insert a blank CD-R, then press Refresh.");
            } else if (!profileSupported) {
                ImGui::TextDisabled("The inserted disc is not CD-R media.");
            } else if (!blankSupported) {
                ImGui::TextDisabled("The inserted CD-R is not blank.");
            } else if (!writerSupported) {
                ImGui::TextDisabled("The selected optical drive cannot write CD-R media.");
            } else if (!backendReady) {
                ImGui::TextDisabled("Could not map this drive to cdrecord's SPTI backend.");
            }
        }

        if (!burn.log.empty()) {
            if (burn.stage == BurnStage::Failed &&
                state.lastBurnStage != BurnStage::Failed) {
                ImGui::SetNextItemOpen(true);
            }
            if (ImGui::CollapsingHeader("Burn log")) {
                if (ImGui::Button("Copy log")) {
                    ImGui::SetClipboardText(burn.log.c_str());
                }
                ImGui::BeginChild(
                    "BurnLogText",
                    ImVec2(0.0F, 145.0F),
                    ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(burn.log.c_str());
                ImGui::EndChild();
            }
        }
        state.lastBurnStage = burn.stage;

        if (ImGui::BeginPopupModal(
                "Confirm burn", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("This will permanently write the selected CD-R.");
            if (drive != nullptr) {
                ImGui::Text("Burner: %s", drive->DisplayName().c_str());
                const int selectedSpeed = SelectedSpeedX(state, drive);
                ImGui::Text(
                    "Speed: %s",
                    selectedSpeed == 0
                        ? "Automatic - firmware negotiates"
                        : (std::to_string(selectedSpeed) + "x requested").c_str());
                if (!drive->blankMediaKnown) {
                    ImGui::TextColored(
                        ImVec4(1.00F, 0.62F, 0.24F, 1.00F),
                        "The drive did not report blank state; cdrecord will verify it before writing.");
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Burn now", ImVec2(150.0F, 0.0F))) {
                if (drive != nullptr) {
                    BurnRequest request;
                    request.cdiPath = state.selectedCdi;
                    request.cdrecordDevice = drive->cdrecordDevice;
                    request.requestedSpeedX = SelectedSpeedX(state, drive);
                    request.checkOnly = false;
                    if (burnEngine.Start(std::move(request))) {
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0F, 0.0F))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        ImGui::EndTable();
    }

    ImGui::End();
}

bool CreateDeviceD3D(const HWND window) {
    DXGI_SWAP_CHAIN_DESC swapChainDescription{};
    swapChainDescription.BufferCount = 2;
    swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescription.OutputWindow = window;
    swapChainDescription.SampleDesc.Count = 1;
    swapChainDescription.Windowed = TRUE;
    swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL createdLevel{};
    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        requestedLevels,
        static_cast<UINT>(std::size(requestedLevels)),
        D3D11_SDK_VERSION,
        &swapChainDescription,
        &g_swapChain,
        &g_device,
        &createdLevel,
        &g_deviceContext);
    return SUCCEEDED(result);
}

void CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (g_swapChain != nullptr &&
        SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_renderTarget);
    }
}

void CleanupDeviceD3D() {
    g_renderTarget.Reset();
    g_swapChain.Reset();
    g_deviceContext.Reset();
    g_device.Reset();
}

} // namespace

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

LRESULT WINAPI WindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        if (g_device != nullptr && wParam != SIZE_MINIMIZED) {
            g_renderTarget.Reset();
            g_swapChain->ResizeBuffers(
                0,
                static_cast<UINT>(LOWORD(lParam)),
                static_cast<UINT>(HIWORD(lParam)),
                DXGI_FORMAT_UNKNOWN,
                0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DEVICECHANGE:
        if (wParam == DBT_DEVICEARRIVAL ||
            wParam == DBT_DEVICEREMOVECOMPLETE ||
            wParam == DBT_DEVNODES_CHANGED) {
            g_driveRefreshRequested = true;
        }
        return 0;
    case WM_DROPFILES: {
        const HDROP drop = reinterpret_cast<HDROP>(wParam);
        const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        if (length > 0) {
            std::wstring path(static_cast<std::size_t>(length) + 1U, L'\0');
            DragQueryFileW(drop, 0, path.data(), length + 1U);
            path.resize(length);
            g_droppedPath = std::move(path);
        }
        DragFinish(drop);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0U) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_CLOSE:
        if (g_jobInProgress) {
            MessageBoxW(
                window,
                L"Dreamcast Burner is still working. Keep the app open until the current operation finishes; closing during a write can ruin the disc.",
                L"Operation in progress",
                MB_OK | MB_ICONWARNING);
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(
    const HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT comResult = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"DreamcastBurnerWindow";
    RegisterClassExW(&windowClass);

    RECT desiredSize{0, 0, 1120, 760};
    AdjustWindowRectEx(&desiredSize, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const HWND window = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Dreamcast Burner",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredSize.right - desiredSize.left,
        desiredSize.bottom - desiredSize.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr || !CreateDeviceD3D(window)) {
        CleanupDeviceD3D();
        if (window != nullptr) {
            DestroyWindow(window);
        }
        UnregisterClassW(windowClass.lpszClassName, instance);
        if (SUCCEEDED(comResult)) {
            CoUninitialize();
        }
        MessageBoxW(
            nullptr,
            L"Direct3D 11 could not be initialized.",
            L"Dreamcast Burner",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    CreateRenderTarget();
    // The burner runs elevated for direct SPTI access. Permit Explorer's
    // lower-integrity file-drop messages so drag-and-drop still works.
    ChangeWindowMessageFilterEx(window, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(window, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(window, 0x0049, MSGFLT_ALLOW, nullptr); // WM_COPYGLOBALDATA
    DragAcceptFiles(window, TRUE);
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureImGuiStyle();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_device.Get(), g_deviceContext.Get());

    const Texture artwork = LoadPngResource(g_device.Get(), IDR_PNG_BURNING_DC);
    const Texture disc = LoadPngResource(g_device.Get(), IDR_PNG_SPINNING_DISC);

    AppState state;
    BurnEngine burnEngine;
    RefreshDrives(state);

    bool running = true;
    while (running) {
        g_jobInProgress = burnEngine.Snapshot().busy;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        if (g_driveRefreshRequested) {
            if (!burnEngine.Snapshot().busy) {
                g_driveRefreshRequested = false;
                RefreshDrives(state);
            }
        }
        if (!g_droppedPath.empty()) {
            if (!burnEngine.Snapshot().busy) {
                SelectCdi(state, burnEngine, std::move(g_droppedPath));
            }
            g_droppedPath.clear();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawApp(state, burnEngine, artwork, disc);

        ImGui::Render();
        constexpr float clearColor[4] = {0.0F, 0.0F, 0.0F, 1.0F};
        g_deviceContext->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
        g_deviceContext->ClearRenderTargetView(g_renderTarget.Get(), clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName, instance);
    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
    return 0;
}
