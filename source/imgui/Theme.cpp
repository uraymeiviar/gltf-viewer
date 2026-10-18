// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/Theme.h"
#include "imgui/StdFileSystem.h"
#include <filesystem>

namespace ux3d::imgui {

static ImFont* s_uiFont = nullptr;
static ImFont* s_monoFont = nullptr;

static std::string resolveFontPath(const std::string& fontName)
{
    auto& fs = StdFileSystem::instance();
    auto exeDir = fs.getExecutableDirPath();
    auto p1 = exeDir / "resources" / "fonts" / fontName;
    if (fs.isFile(p1)) return p1.toString().c_str();

#ifdef UX3D_RENDERER_RESOURCES_DIR
    auto compileTimePath = fs.toPath(UX3D_RENDERER_RESOURCES_DIR);
    auto p2 = compileTimePath.getParent() / "resources" / "fonts" / fontName;
    if (fs.isFile(p2)) return p2.toString().c_str();
#endif

    std::filesystem::path localRel = std::filesystem::path("resources") / "fonts" / fontName;
    if (std::filesystem::exists(localRel)) return localRel.string();

    std::string directAssetPath = "F:/new-fss-aircraft-ejet/assets/wasmdata.vfs/fonts/" + fontName;
    if (std::filesystem::exists(directAssetPath)) return directAssetPath;

    return fontName;
}

void applyFssTheme()
{
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    // FSS Aviation Theme with Emerald Accent (#21AB6B)
    colors[ImGuiCol_Text]                      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    colors[ImGuiCol_TextDisabled]              = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]                  = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ChildBg]                   = ImVec4(0.10f, 0.10f, 0.12f, 0.45f);
    colors[ImGuiCol_PopupBg]                   = ImVec4(0.18f, 0.18f, 0.20f, 0.98f);
    colors[ImGuiCol_Border]                    = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    colors[ImGuiCol_BorderShadow]              = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    colors[ImGuiCol_FrameBg]                   = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
    colors[ImGuiCol_FrameBgHovered]            = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
    colors[ImGuiCol_FrameBgActive]             = ImVec4(0.19f, 0.36f, 0.28f, 0.67f);
    colors[ImGuiCol_TitleBg]                   = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive]             = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]          = ImVec4(0.12f, 0.12f, 0.14f, 0.75f);
    colors[ImGuiCol_MenuBarBg]                 = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]             = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
    colors[ImGuiCol_ScrollbarGrabHovered]      = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]       = ImVec4(0.19f, 0.36f, 0.28f, 1.00f);
    colors[ImGuiCol_CheckMark]                 = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_CheckboxSelectedBg]        = ImVec4(0.13f, 0.67f, 0.42f, 0.25f);
    colors[ImGuiCol_SliderGrab]                = ImVec4(0.20f, 0.20f, 0.24f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]          = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_Button]                    = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]             = ImVec4(0.17f, 0.56f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive]              = ImVec4(0.11f, 0.79f, 0.48f, 1.00f);
    colors[ImGuiCol_Header]                    = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_HeaderHovered]             = ImVec4(0.13f, 0.67f, 0.42f, 0.34f);
    colors[ImGuiCol_HeaderActive]              = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_Separator]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
    colors[ImGuiCol_SeparatorHovered]          = ImVec4(0.19f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_SeparatorActive]           = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_ResizeGrip]                = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]         = ImVec4(0.19f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]          = ImVec4(0.34f, 0.81f, 0.56f, 0.95f);
    colors[ImGuiCol_TabHovered]                = ImVec4(0.17f, 0.56f, 0.38f, 1.00f);
    colors[ImGuiCol_Tab]                       = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
    colors[ImGuiCol_TabSelected]               = ImVec4(0.10f, 0.10f, 0.12f, 0.85f);
    colors[ImGuiCol_TabSelectedOverline]       = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_TabDimmed]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
    colors[ImGuiCol_TabDimmedSelected]         = ImVec4(0.00f, 0.00f, 0.00f, 0.71f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview]            = ImVec4(0.13f, 0.67f, 0.42f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]            = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_PlotLines]                 = ImVec4(0.57f, 0.57f, 0.57f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]          = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]             = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]      = ImVec4(0.17f, 0.79f, 0.50f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.45f);
    colors[ImGuiCol_TableBorderStrong]         = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    colors[ImGuiCol_TableBorderLight]          = ImVec4(0.00f, 0.00f, 0.00f, 0.18f);
    colors[ImGuiCol_TableRowBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.15f);
    colors[ImGuiCol_TableRowBgAlt]             = ImVec4(0.00f, 0.00f, 0.00f, 0.08f);
    colors[ImGuiCol_TextLink]                  = ImVec4(0.34f, 0.81f, 0.56f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]            = ImVec4(0.19f, 0.36f, 0.28f, 1.00f);
    colors[ImGuiCol_DragDropTarget]            = ImVec4(0.13f, 0.67f, 0.42f, 0.90f);
    colors[ImGuiCol_NavHighlight]              = ImVec4(0.13f, 0.67f, 0.42f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]     = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

    style->ChildRounding      = 6.0f;
    style->TabRounding        = 6.0f;
    style->WindowRounding     = 6.0f;
    style->FrameRounding      = 4.0f;
    style->PopupRounding      = 5.0f;
    style->ScrollbarRounding  = 6.0f;
    style->GrabRounding       = 4.0f;

    style->ScrollbarSize      = 14.0f;
    style->GrabMinSize        = 14.0f;

    style->WindowBorderSize   = 1.0f;
    style->ChildBorderSize    = 0.0f;
    style->PopupBorderSize    = 1.0f;
    style->FrameBorderSize    = 1.0f;
    style->TabBorderSize      = 0.0f;
    style->TabBarBorderSize   = 0.0f;

    style->IndentSpacing      = 20.0f;
    style->WindowPadding      = ImVec2(6.0f, 6.0f);
    style->FramePadding       = ImVec2(6.0f, 5.0f);
    style->CellPadding        = ImVec2(4.0f, 3.0f);
    style->ItemSpacing        = ImVec2(6.0f, 6.0f);
    style->ItemInnerSpacing   = ImVec2(6.0f, 4.0f);
    style->WindowTitleAlign   = ImVec2(0.5f, 0.5f);
    style->WindowMenuButtonPosition = ImGuiDir_None;

    style->DockingSeparatorSize = 4.0f;
}

void loadFssFonts(ImGuiIO& io)
{
    static const ImWchar s_glyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0100, 0x024F, // Latin Extended-A + Latin Extended-B
        ICON_MIN_MDI, ICON_MAX_MDI, // Material Design Icons (0xE000 - 0xFD17)
        0,
    };

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;

    std::string uiFontPath = resolveFontPath("fss-ui.ttf");
    if (std::filesystem::exists(uiFontPath)) {
        s_uiFont = io.Fonts->AddFontFromFileTTF(uiFontPath.c_str(), 16.0f, &fontConfig, s_glyphRanges);
    }
    if (s_uiFont == nullptr) {
        s_uiFont = io.Fonts->AddFontDefault();
    }

    std::string monoFontPath = resolveFontPath("fss-ui-mono.ttf");
    if (std::filesystem::exists(monoFontPath)) {
        s_monoFont = io.Fonts->AddFontFromFileTTF(monoFontPath.c_str(), 15.0f, &fontConfig, s_glyphRanges);
    }
    if (s_monoFont == nullptr) {
        s_monoFont = s_uiFont;
    }
}

ImFont* getUiFont()
{
    return s_uiFont;
}

ImFont* getMonoFont()
{
    return s_monoFont;
}

} // namespace ux3d::imgui
