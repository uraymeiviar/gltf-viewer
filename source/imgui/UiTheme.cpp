// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/UiTheme.h"
#include "imgui/Theme.h"
#include "imgui/SettingsFile.h"
#include "imgui/UiProbe.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ux3d::imgui {

namespace {

constexpr const char* kSection = "UiTheme";
constexpr const char* kThemePrefix = "UiTheme.";
constexpr const char* kDefaultName = "Default";
constexpr const char* kModifiedName = "User Modified";
constexpr const char* kNewThemeName = "My Theme";

enum class FieldType { Float, Vec2, Int, Bool };

struct Field {
    const char* name;
    FieldType type;
    std::size_t offset;
};

#define GLIDER_STYLE_FIELD(type, member) Field{#member, FieldType::type, offsetof(ImGuiStyle, member)}

// Every user-facing ImGuiStyle field the editor changes. FontScaleDpi is left out: it follows the monitor.
const Field kFields[] = {
    GLIDER_STYLE_FIELD(Float, FontSizeBase),
    GLIDER_STYLE_FIELD(Float, FontScaleMain),
    GLIDER_STYLE_FIELD(Float, Alpha),
    GLIDER_STYLE_FIELD(Float, DisabledAlpha),
    GLIDER_STYLE_FIELD(Vec2, WindowPadding),
    GLIDER_STYLE_FIELD(Float, WindowRounding),
    GLIDER_STYLE_FIELD(Float, WindowBorderSize),
    GLIDER_STYLE_FIELD(Float, WindowBorderHoverPadding),
    GLIDER_STYLE_FIELD(Vec2, WindowMinSize),
    GLIDER_STYLE_FIELD(Vec2, WindowTitleAlign),
    GLIDER_STYLE_FIELD(Int, WindowMenuButtonPosition),
    GLIDER_STYLE_FIELD(Float, ChildRounding),
    GLIDER_STYLE_FIELD(Float, ChildBorderSize),
    GLIDER_STYLE_FIELD(Float, PopupRounding),
    GLIDER_STYLE_FIELD(Float, PopupBorderSize),
    GLIDER_STYLE_FIELD(Vec2, FramePadding),
    GLIDER_STYLE_FIELD(Float, FrameRounding),
    GLIDER_STYLE_FIELD(Float, FrameBorderSize),
    GLIDER_STYLE_FIELD(Vec2, ItemSpacing),
    GLIDER_STYLE_FIELD(Vec2, ItemInnerSpacing),
    GLIDER_STYLE_FIELD(Vec2, CellPadding),
    GLIDER_STYLE_FIELD(Vec2, TouchExtraPadding),
    GLIDER_STYLE_FIELD(Float, IndentSpacing),
    GLIDER_STYLE_FIELD(Float, ColumnsMinSpacing),
    GLIDER_STYLE_FIELD(Float, ScrollbarSize),
    GLIDER_STYLE_FIELD(Float, ScrollbarRounding),
    GLIDER_STYLE_FIELD(Float, ScrollbarPadding),
    GLIDER_STYLE_FIELD(Float, GrabMinSize),
    GLIDER_STYLE_FIELD(Float, GrabRounding),
    GLIDER_STYLE_FIELD(Float, LogSliderDeadzone),
    GLIDER_STYLE_FIELD(Float, ImageRounding),
    GLIDER_STYLE_FIELD(Float, ImageBorderSize),
    GLIDER_STYLE_FIELD(Float, TabRounding),
    GLIDER_STYLE_FIELD(Float, TabBorderSize),
    GLIDER_STYLE_FIELD(Float, TabMinWidthBase),
    GLIDER_STYLE_FIELD(Float, TabMinWidthShrink),
    GLIDER_STYLE_FIELD(Float, TabCloseButtonMinWidthSelected),
    GLIDER_STYLE_FIELD(Float, TabCloseButtonMinWidthUnselected),
    GLIDER_STYLE_FIELD(Float, TabBarBorderSize),
    GLIDER_STYLE_FIELD(Float, TabBarOverlineSize),
    GLIDER_STYLE_FIELD(Float, TableAngledHeadersAngle),
    GLIDER_STYLE_FIELD(Vec2, TableAngledHeadersTextAlign),
    GLIDER_STYLE_FIELD(Int, TreeLinesFlags),
    GLIDER_STYLE_FIELD(Float, TreeLinesSize),
    GLIDER_STYLE_FIELD(Float, TreeLinesRounding),
    GLIDER_STYLE_FIELD(Float, MenuItemRounding),
    GLIDER_STYLE_FIELD(Float, SelectableRounding),
    GLIDER_STYLE_FIELD(Float, DragDropTargetRounding),
    GLIDER_STYLE_FIELD(Float, DragDropTargetBorderSize),
    GLIDER_STYLE_FIELD(Float, DragDropTargetPadding),
    GLIDER_STYLE_FIELD(Float, ColorMarkerSize),
    GLIDER_STYLE_FIELD(Int, ColorButtonPosition),
    GLIDER_STYLE_FIELD(Vec2, ButtonTextAlign),
    GLIDER_STYLE_FIELD(Vec2, SelectableTextAlign),
    GLIDER_STYLE_FIELD(Float, InputTextCursorSize),
    GLIDER_STYLE_FIELD(Float, SeparatorSize),
    GLIDER_STYLE_FIELD(Float, SeparatorTextBorderSize),
    GLIDER_STYLE_FIELD(Vec2, SeparatorTextAlign),
    GLIDER_STYLE_FIELD(Vec2, SeparatorTextPadding),
    GLIDER_STYLE_FIELD(Vec2, DisplayWindowPadding),
    GLIDER_STYLE_FIELD(Vec2, DisplaySafeAreaPadding),
    GLIDER_STYLE_FIELD(Bool, DockingNodeHasCloseButton),
    GLIDER_STYLE_FIELD(Float, DockingSeparatorSize),
    GLIDER_STYLE_FIELD(Float, MouseCursorScale),
    GLIDER_STYLE_FIELD(Bool, AntiAliasedLines),
    GLIDER_STYLE_FIELD(Bool, AntiAliasedLineEnds),
    GLIDER_STYLE_FIELD(Bool, AntiAliasedFill),
    GLIDER_STYLE_FIELD(Float, CurveTessellationMaxError),
    GLIDER_STYLE_FIELD(Float, CircleTessellationMaxError),
    GLIDER_STYLE_FIELD(Float, HoverStationaryDelay),
    GLIDER_STYLE_FIELD(Float, HoverDelayShort),
    GLIDER_STYLE_FIELD(Float, HoverDelayNormal),
    GLIDER_STYLE_FIELD(Int, HoverFlagsForTooltipMouse),
    GLIDER_STYLE_FIELD(Int, HoverFlagsForTooltipNav),
};

#undef GLIDER_STYLE_FIELD

static_assert(sizeof(ImGuiDir) == sizeof(int) && sizeof(ImGuiTreeNodeFlags) == sizeof(int) &&
              sizeof(ImGuiHoveredFlags) == sizeof(int));

struct Theme {
    std::string name;
    std::vector<SettingsEntry> entries; // Differences from Default.
};

ImGuiStyle s_default;              // The built-in theme, set by apply().
ImGuiStyle s_baseStyle;            // The selected theme as saved (for "User Modified": the theme it came from).
std::vector<Theme> s_themes;       // Stored themes in settings.ini order, "User Modified" included.
std::string s_current = kDefaultName;
std::string s_base = kDefaultName; // Only meaningful while s_current is "User Modified".
std::string s_appliedText;         // Joined differences of the style as last applied or written.
char s_nameBuffer[64] = {};
bool s_applied = false;

std::string formatFloat(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(v));
    return buf;
}

std::string formatField(const ImGuiStyle& style, const Field& field)
{
    const auto* base = reinterpret_cast<const unsigned char*>(&style) + field.offset;
    switch (field.type) {
    case FieldType::Float: return formatFloat(*reinterpret_cast<const float*>(base));
    case FieldType::Vec2: {
        const ImVec2& v = *reinterpret_cast<const ImVec2*>(base);
        return formatFloat(v.x) + "," + formatFloat(v.y);
    }
    case FieldType::Int: return std::to_string(*reinterpret_cast<const int*>(base));
    case FieldType::Bool: return *reinterpret_cast<const bool*>(base) ? "1" : "0";
    }
    return {};
}

bool parseField(ImGuiStyle& style, const Field& field, const std::string& text)
{
    auto* base = reinterpret_cast<unsigned char*>(&style) + field.offset;
    switch (field.type) {
    case FieldType::Float: {
        float v = 0.0f;
        if (std::sscanf(text.c_str(), "%f", &v) != 1) return false;
        *reinterpret_cast<float*>(base) = v;
        return true;
    }
    case FieldType::Vec2: {
        ImVec2 v;
        if (std::sscanf(text.c_str(), "%f,%f", &v.x, &v.y) != 2) return false;
        *reinterpret_cast<ImVec2*>(base) = v;
        return true;
    }
    case FieldType::Int: {
        int v = 0;
        if (std::sscanf(text.c_str(), "%d", &v) != 1) return false;
        *reinterpret_cast<int*>(base) = v;
        return true;
    }
    case FieldType::Bool:
        if (text != "0" && text != "1") return false;
        *reinterpret_cast<bool*>(base) = text == "1";
        return true;
    }
    return false;
}

std::string formatColor(const ImVec4& c)
{
    return formatFloat(c.x) + "," + formatFloat(c.y) + "," + formatFloat(c.z) + "," + formatFloat(c.w);
}

std::vector<SettingsEntry> differencesOf(const ImGuiStyle& style)
{
    std::vector<SettingsEntry> entries;
    for (const Field& field : kFields) {
        std::string value = formatField(style, field);
        if (value != formatField(s_default, field)) entries.emplace_back(field.name, std::move(value));
    }
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        std::string value = formatColor(style.Colors[i]);
        if (value != formatColor(s_default.Colors[i]))
            entries.emplace_back(std::string("Color.") + ImGui::GetStyleColorName(i), std::move(value));
    }
    return entries;
}

std::string joined(const std::vector<SettingsEntry>& entries)
{
    std::string text;
    for (const auto& [key, value] : entries) text += key + "=" + value + "\n";
    return text;
}

// Default plus a theme's differences; unknown keys and bad values are skipped with a warning.
ImGuiStyle styleFrom(const std::string& themeName, const std::vector<SettingsEntry>& entries, bool warn)
{
    ImGuiStyle style = s_default;
    for (const auto& [key, value] : entries) {
        if (key == "version") continue;
        bool ok = false;
        if (key.rfind("Color.", 0) == 0) {
            const std::string name = key.substr(6);
            for (int i = 0; i < ImGuiCol_COUNT; ++i) {
                if (name != ImGui::GetStyleColorName(i)) continue;
                ImVec4 c;
                ok = std::sscanf(value.c_str(), "%f,%f,%f,%f", &c.x, &c.y, &c.z, &c.w) == 4;
                if (ok) style.Colors[i] = c;
                break;
            }
        } else {
            for (const Field& field : kFields) {
                if (key == field.name) {
                    ok = parseField(style, field, value);
                    break;
                }
            }
        }
        if (!ok && warn)
            std::fprintf(stderr, "[UiTheme] Ignoring settings.ini [%s%s] %s=%s\n", kThemePrefix, themeName.c_str(),
                         key.c_str(), value.c_str());
    }
    return style;
}

Theme* findTheme(const std::string& name)
{
    for (Theme& theme : s_themes)
        if (theme.name == name) return &theme;
    return nullptr;
}

ImGuiStyle styleOfTheme(const std::string& name)
{
    const Theme* theme = name == kDefaultName ? nullptr : findTheme(name);
    return theme ? styleFrom(name, theme->entries, false) : s_default;
}

std::string lower(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string trimmed(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// Empty when the name can be saved, else why not.
std::string nameProblem(const std::string& name)
{
    if (name.empty()) return "Enter a theme name.";
    if (lower(name) == lower(kDefaultName) || lower(name) == lower(kModifiedName))
        return "\"" + name + "\" is reserved.";
    for (char c : name)
        if (c == '[' || c == ']' || static_cast<unsigned char>(c) < 0x20) return "The name cannot contain [ ] or control characters.";
    return {};
}

void writeSettings()
{
    SettingsFile& settings = SettingsFile::instance();
    if (!settings.persistenceEnabled()) return; // --isolated: themes stay in memory for the session.
    settings.load();
    for (const std::string& section : settings.sectionNames())
        if (section.rfind(kThemePrefix, 0) == 0 && !findTheme(section.substr(std::strlen(kThemePrefix))))
            settings.removeSection(section);
    for (const Theme& theme : s_themes) {
        std::vector<SettingsEntry> entries{{"version", "1"}}; // Keeps a theme equal to Default from being dropped.
        entries.insert(entries.end(), theme.entries.begin(), theme.entries.end());
        settings.replaceSection(kThemePrefix + theme.name, std::move(entries));
    }
    std::vector<SettingsEntry> state;
    if (s_current != kDefaultName) state.emplace_back("current", s_current);
    if (s_current == kModifiedName && s_base != kDefaultName) state.emplace_back("base", s_base);
    settings.replaceSection(kSection, std::move(state)); // Empty removes the section.
    settings.save();
}

void setNameBuffer(const std::string& name)
{
    std::snprintf(s_nameBuffer, sizeof(s_nameBuffer), "%s", name.c_str());
}

void applyStyle(const ImGuiStyle& source)
{
    ImGuiStyle& style = ImGui::GetStyle();
    const float dpi = style.FontScaleDpi; // Follows the monitor, not the theme.
    style = source;
    style.FontScaleDpi = dpi;
    style._NextFrameFontSizeBase = style.FontSizeBase;
    s_appliedText = joined(differencesOf(style));
}

void selectTheme(const std::string& name)
{
    s_current = name;
    if (name == kModifiedName) {
        if (s_base != kDefaultName && !findTheme(s_base)) s_base = kDefaultName;
        s_baseStyle = styleOfTheme(s_base);
        setNameBuffer(s_base == kDefaultName ? kNewThemeName : s_base);
    } else {
        s_base = kDefaultName;
        s_baseStyle = styleOfTheme(name);
    }
    applyStyle(styleOfTheme(name));
}

// An edit: the style now differs from what was applied. It becomes (or updates) "User Modified".
void recordEdit(std::vector<SettingsEntry> entries)
{
    if (s_current != kModifiedName) {
        s_base = s_current;
        // s_baseStyle already holds the theme being modified.
        setNameBuffer(s_base == kDefaultName ? kNewThemeName : s_base);
        s_current = kModifiedName;
    }
    if (Theme* modified = findTheme(kModifiedName)) modified->entries = std::move(entries);
    else s_themes.push_back({kModifiedName, std::move(entries)});
    s_appliedText = joined(differencesOf(ImGui::GetStyle()));
    writeSettings();
}

void saveModifiedAs(const std::string& name)
{
    std::vector<SettingsEntry> entries = differencesOf(ImGui::GetStyle());
    if (Theme* existing = findTheme(name)) existing->entries = std::move(entries);
    else s_themes.push_back({name, std::move(entries)});
    std::erase_if(s_themes, [](const Theme& t) { return t.name == kModifiedName; });
    s_current = name;
    s_base = kDefaultName;
    s_baseStyle = ImGui::GetStyle();
    writeSettings();
}

void deleteTheme(const std::string& name)
{
    std::erase_if(s_themes, [&](const Theme& t) { return t.name == name; });
    if (s_base == name) s_base = kDefaultName;
    selectTheme(kDefaultName);
    writeSettings();
}

// ------------------------------------------------------------------------------------------------
// The style editor: Dear ImGui's ShowStyleEditor (imgui_demo.cpp, MIT license) without its built-in color presets,
// font selector, Save/Revert Ref and color Export (themes replace them). Per-color Revert restores the base theme's color.
// ------------------------------------------------------------------------------------------------

void helpMarker(const char* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

const char* treeLinesFlagsName(ImGuiTreeNodeFlags flags)
{
    if (flags == ImGuiTreeNodeFlags_DrawLinesNone) return "DrawLinesNone";
    if (flags == ImGuiTreeNodeFlags_DrawLinesFull) return "DrawLinesFull";
    if (flags == ImGuiTreeNodeFlags_DrawLinesToNodes) return "DrawLinesToNodes";
    return "";
}

void drawStyleEditor(ImGuiStyle& style, const ImGuiStyle& ref)
{
    using namespace ImGui;
    const float defaultBorderSize = static_cast<float>(static_cast<int>(style._MainScale));
    const float maxBorderSize = std::max(defaultBorderSize, 2.0f);

    PushItemWidth(GetWindowWidth() * 0.50f);

    SeparatorText("General");
    if (DragFloat("FontSizeBase", &style.FontSizeBase, 0.20f, 5.0f, 100.0f, "%.0f"))
        style._NextFrameFontSizeBase = style.FontSizeBase;
    SameLine(0.0f, 0.0f);
    Text(" (out %.2f)", GetFontSize());
    DragFloat("FontScaleMain", &style.FontScaleMain, 0.02f, 0.5f, 4.0f);
    if (SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f"))
        style.GrabRounding = style.FrameRounding;
    UiProbe::item("settings.theme.framerounding", 0);
    { bool border = style.WindowBorderSize > 0.0f; if (Checkbox("WindowBorder", &border)) style.WindowBorderSize = border ? defaultBorderSize : 0.0f; }
    SameLine();
    { bool border = style.FrameBorderSize > 0.0f; if (Checkbox("FrameBorder", &border)) style.FrameBorderSize = border ? defaultBorderSize : 0.0f; }
    SameLine();
    { bool border = style.PopupBorderSize > 0.0f; if (Checkbox("PopupBorder", &border)) style.PopupBorderSize = border ? defaultBorderSize : 0.0f; }

    SeparatorText("Details");
    if (BeginTabBar("##themeTabs")) {
        if (BeginTabItem("Sizes")) {
            SeparatorText("Main");
            SliderFloat2("WindowPadding", reinterpret_cast<float*>(&style.WindowPadding), 0.0f, 20.0f, "%.0f");
            SliderFloat2("FramePadding", reinterpret_cast<float*>(&style.FramePadding), 0.0f, 20.0f, "%.0f");
            SliderFloat2("ItemSpacing", reinterpret_cast<float*>(&style.ItemSpacing), 0.0f, 20.0f, "%.0f");
            SliderFloat2("ItemInnerSpacing", reinterpret_cast<float*>(&style.ItemInnerSpacing), 0.0f, 20.0f, "%.0f");
            SliderFloat2("TouchExtraPadding", reinterpret_cast<float*>(&style.TouchExtraPadding), 0.0f, 10.0f, "%.0f");
            SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
            SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");

            SeparatorText("Borders");
            SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, maxBorderSize, "%.0f");

            SeparatorText("Rounding");
            SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("MenuItemRounding", &style.MenuItemRounding, 0.0f, 12.0f, "%.0f");

            SeparatorText("Scrollbar");
            SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
            SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("ScrollbarPadding", &style.ScrollbarPadding, 0.0f, 10.0f, "%.0f");

            SeparatorText("Tabs");
            SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("TabBarOverlineSize", &style.TabBarOverlineSize, 0.0f, std::max(3.0f, maxBorderSize), "%.0f");
            helpMarker("Overline is only drawn over the selected tab when ImGuiTabBarFlags_DrawSelectedOverline is set.");
            DragFloat("TabMinWidthBase", &style.TabMinWidthBase, 0.5f, 1.0f, 500.0f, "%.0f");
            DragFloat("TabMinWidthShrink", &style.TabMinWidthShrink, 0.5f, 1.0f, 500.0f, "%.0f");
            DragFloat("TabCloseButtonMinWidthSelected", &style.TabCloseButtonMinWidthSelected, 0.5f, -1.0f, 100.0f,
                      style.TabCloseButtonMinWidthSelected < 0.0f ? "%.0f (Always)" : "%.0f");
            DragFloat("TabCloseButtonMinWidthUnselected", &style.TabCloseButtonMinWidthUnselected, 0.5f, -1.0f, 100.0f,
                      style.TabCloseButtonMinWidthUnselected < 0.0f ? "%.0f (Always)" : "%.0f");
            SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f, "%.0f");

            SeparatorText("Tables");
            SliderFloat2("CellPadding", reinterpret_cast<float*>(&style.CellPadding), 0.0f, 20.0f, "%.0f");
            SliderAngle("TableAngledHeadersAngle", &style.TableAngledHeadersAngle, -50.0f, +50.0f);
            SliderFloat2("TableAngledHeadersTextAlign", reinterpret_cast<float*>(&style.TableAngledHeadersTextAlign), 0.0f, 1.0f, "%.2f");

            SeparatorText("Trees");
            if (BeginCombo("TreeLinesFlags", treeLinesFlagsName(style.TreeLinesFlags))) {
                const ImGuiTreeNodeFlags options[] = {ImGuiTreeNodeFlags_DrawLinesNone, ImGuiTreeNodeFlags_DrawLinesFull,
                                                      ImGuiTreeNodeFlags_DrawLinesToNodes};
                for (ImGuiTreeNodeFlags option : options)
                    if (Selectable(treeLinesFlagsName(option), style.TreeLinesFlags == option)) style.TreeLinesFlags = option;
                EndCombo();
            }
            SliderFloat("TreeLinesSize", &style.TreeLinesSize, 0.0f, maxBorderSize, "%.0f");
            SliderFloat("TreeLinesRounding", &style.TreeLinesRounding, 0.0f, 12.0f, "%.0f");

            SeparatorText("Windows");
            SliderFloat2("WindowTitleAlign", reinterpret_cast<float*>(&style.WindowTitleAlign), 0.0f, 1.0f, "%.2f");
            SliderFloat("WindowBorderHoverPadding", &style.WindowBorderHoverPadding, 1.0f, 20.0f, "%.0f");
            int menuButtonPosition = style.WindowMenuButtonPosition + 1;
            if (Combo("WindowMenuButtonPosition", &menuButtonPosition, "None\0Left\0Right\0"))
                style.WindowMenuButtonPosition = static_cast<ImGuiDir>(menuButtonPosition - 1);

            SeparatorText("Widgets");
            SliderFloat("ColorMarkerSize", &style.ColorMarkerSize, 0.0f, 8.0f, "%.0f");
            Combo("ColorButtonPosition", reinterpret_cast<int*>(&style.ColorButtonPosition), "Left\0Right\0");
            SliderFloat2("ButtonTextAlign", reinterpret_cast<float*>(&style.ButtonTextAlign), 0.0f, 1.0f, "%.2f");
            helpMarker("Alignment applies when a button is larger than its text content.");
            SliderFloat2("SelectableTextAlign", reinterpret_cast<float*>(&style.SelectableTextAlign), 0.0f, 1.0f, "%.2f");
            helpMarker("Alignment applies when a selectable is larger than its text content.");
            SliderFloat("SeparatorSize", &style.SeparatorSize, 0.0f, 10.0f, "%.0f");
            SliderFloat("SeparatorTextBorderSize", &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
            SliderFloat2("SeparatorTextAlign", reinterpret_cast<float*>(&style.SeparatorTextAlign), 0.0f, 1.0f, "%.2f");
            SliderFloat2("SeparatorTextPadding", reinterpret_cast<float*>(&style.SeparatorTextPadding), 0.0f, 40.0f, "%.0f");
            SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");
            SliderFloat("ImageRounding", &style.ImageRounding, 0.0f, 12.0f, "%.0f");
            SliderFloat("ImageBorderSize", &style.ImageBorderSize, 0.0f, maxBorderSize, "%.0f");

            SeparatorText("Docking");
            Checkbox("DockingNodeHasCloseButton", &style.DockingNodeHasCloseButton);
            SliderFloat("DockingSeparatorSize", &style.DockingSeparatorSize, 0.0f, 12.0f, "%.0f");

            SeparatorText("Tooltips");
            for (int n = 0; n < 2; n++) {
                if (TreeNodeEx(n == 0 ? "HoverFlagsForTooltipMouse" : "HoverFlagsForTooltipNav")) {
                    ImGuiHoveredFlags* p = n == 0 ? &style.HoverFlagsForTooltipMouse : &style.HoverFlagsForTooltipNav;
                    CheckboxFlags("ImGuiHoveredFlags_DelayNone", p, ImGuiHoveredFlags_DelayNone);
                    CheckboxFlags("ImGuiHoveredFlags_DelayShort", p, ImGuiHoveredFlags_DelayShort);
                    CheckboxFlags("ImGuiHoveredFlags_DelayNormal", p, ImGuiHoveredFlags_DelayNormal);
                    CheckboxFlags("ImGuiHoveredFlags_Stationary", p, ImGuiHoveredFlags_Stationary);
                    CheckboxFlags("ImGuiHoveredFlags_NoSharedDelay", p, ImGuiHoveredFlags_NoSharedDelay);
                    TreePop();
                }
            }

            SeparatorText("Misc");
            SliderFloat2("DisplayWindowPadding", reinterpret_cast<float*>(&style.DisplayWindowPadding), 0.0f, 30.0f, "%.0f");
            helpMarker("Apply to regular windows: amount which we enforce to keep visible when moving near edges of your screen.");
            SliderFloat2("DisplaySafeAreaPadding", reinterpret_cast<float*>(&style.DisplaySafeAreaPadding), 0.0f, 30.0f, "%.0f");
            helpMarker("Apply to every windows, menus, popups, tooltips: amount where we avoid displaying contents.");
            EndTabItem();
        }

        if (BeginTabItem("Colors")) {
            static ImGuiColorEditFlags alphaFlags = 0;
            if (RadioButton("Opaque", alphaFlags == ImGuiColorEditFlags_AlphaOpaque)) alphaFlags = ImGuiColorEditFlags_AlphaOpaque;
            SameLine();
            if (RadioButton("Alpha", alphaFlags == ImGuiColorEditFlags_None)) alphaFlags = ImGuiColorEditFlags_None;
            SameLine();
            if (RadioButton("Both", alphaFlags == ImGuiColorEditFlags_AlphaPreviewHalf)) alphaFlags = ImGuiColorEditFlags_AlphaPreviewHalf;
            helpMarker("In the color list:\nLeft-click on color square to open color picker,\nRight-click to open edit options menu.");

            static ImGuiTextFilter filter;
            SetNextItemWidth(-FLT_MIN);
            filter.DrawWithHint("##FilterColors", "Filter Colors (incl -excl)");

            SetNextWindowSizeConstraints(ImVec2(0.0f, GetTextLineHeightWithSpacing() * 10), ImVec2(FLT_MAX, FLT_MAX));
            BeginChild("##colors", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
                       ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
            PushItemWidth(GetFontSize() * -12);
            for (int i = 0; i < ImGuiCol_COUNT; i++) {
                const char* name = GetStyleColorName(i);
                if (!filter.PassFilter(name)) continue;
                PushID(i);
                if (Button("?")) DebugFlashStyleColor(static_cast<ImGuiCol>(i));
                SetItemTooltip("Flash given color to identify places where it is used.");
                SameLine();
                ColorEdit4("##color", reinterpret_cast<float*>(&style.Colors[i]), ImGuiColorEditFlags_AlphaBar | alphaFlags);
                if (std::memcmp(&style.Colors[i], &ref.Colors[i], sizeof(ImVec4)) != 0) {
                    SameLine(0.0f, style.ItemInnerSpacing.x);
                    if (Button("Revert")) style.Colors[i] = ref.Colors[i];
                    SetItemTooltip("Restore this color from the theme it was changed from.");
                }
                SameLine(0.0f, style.ItemInnerSpacing.x);
                TextUnformatted(name);
                PopID();
            }
            PopItemWidth();
            EndChild();
            EndTabItem();
        }

        if (BeginTabItem("Rendering")) {
            Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
            helpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");
            BeginDisabled(!style.AntiAliasedLines);
            Checkbox("Anti-aliased line ends", &style.AntiAliasedLineEnds);
            EndDisabled();
            Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
            PushItemWidth(GetFontSize() * 8);
            DragFloat("Curve Tessellation Max Error", &style.CurveTessellationMaxError, 0.02f, 0.10f, 10.0f, "%.2f");
            if (style.CurveTessellationMaxError < 0.10f) style.CurveTessellationMaxError = 0.10f;
            DragFloat("Circle Tessellation Max Error", &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f, "%.2f",
                      ImGuiSliderFlags_AlwaysClamp);
            DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
            DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f, 1.0f, "%.2f");
            helpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
            PopItemWidth();
            EndTabItem();
        }
        EndTabBar();
    }
    PopItemWidth();
}

} // namespace

void UiTheme::apply()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    applyFssTheme();
    // FontSizeBase is 0 until the first frame resolves it from the default font; fix it now so Default is stable.
    if (ImFont* font = getUiFont(); font != nullptr && font->LegacySize > 0.0f) style.FontSizeBase = font->LegacySize;
    s_default = style;
    s_applied = true;

    s_themes.clear();
    std::string current = kDefaultName;
    SettingsFile& settings = SettingsFile::instance();
    if (settings.persistenceEnabled()) { // --isolated: never read.
        settings.load();
        for (const std::string& section : settings.sectionNames()) {
            if (section.rfind(kThemePrefix, 0) != 0) continue;
            const std::string name = section.substr(std::strlen(kThemePrefix));
            if (name.empty() || name == kDefaultName || findTheme(name)) continue;
            std::vector<SettingsEntry> entries = settings.section(section);
            styleFrom(name, entries, true); // Warns once about bad keys.
            std::erase_if(entries, [](const SettingsEntry& e) { return e.first == "version"; });
            s_themes.push_back({name, std::move(entries)});
        }
        current = settings.get(kSection, "current", kDefaultName);
        s_base = settings.get(kSection, "base", kDefaultName);
        if (current != kDefaultName && !findTheme(current)) {
            std::fprintf(stderr, "[UiTheme] settings.ini theme \"%s\" not found; using Default\n", current.c_str());
            current = kDefaultName;
        }
    }
    selectTheme(current);
}

void UiTheme::drawSettings()
{
    if (!s_applied) apply();

    ImGui::BeginChild("UiThemePage", ImVec2(0, 0), false);
    ImGui::TextWrapped("Colors and sizes of the user interface. Edits apply at once and are kept as \"%s\"; save them "
                       "under a name to keep them as a theme. Themes are stored in settings.ini.", kModifiedName);

    // Theme list: Default (built in) and the themes in settings.ini.
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
    if (ImGui::BeginCombo("Theme", s_current.c_str())) {
        int index = 0;
        const auto item = [&](const std::string& name) {
            const bool selected = name == s_current;
            if (ImGui::Selectable(name.c_str(), selected) && !selected) {
                selectTheme(name);
                writeSettings();
            }
            UiProbe::item("settings.theme.item", index++, false, selected);
            if (selected) ImGui::SetItemDefaultFocus();
        };
        item(kDefaultName);
        for (size_t i = 0; i < s_themes.size(); ++i) item(std::string(s_themes[i].name));
        ImGui::EndCombo();
    }
    UiProbe::item("settings.theme.combo", 0);
    if (s_current == kModifiedName) {
        ImGui::SameLine();
        ImGui::TextDisabled("(from %s)", s_base.c_str());
    }
    if (s_current != kDefaultName) {
        ImGui::SameLine();
        const bool modified = s_current == kModifiedName;
        if (ImGui::Button(modified ? "Discard" : "Delete")) {
            if (modified) {
                const std::string base = s_base;
                std::erase_if(s_themes, [](const Theme& t) { return t.name == kModifiedName; });
                selectTheme(base);
                writeSettings();
            } else {
                deleteTheme(s_current);
            }
        }
        UiProbe::item("settings.theme.delete", 0);
        ImGui::SetItemTooltip(modified ? "Drop the unsaved changes and go back to the theme they came from."
                                       : "Remove this theme from settings.ini and use Default.");
    }

    // "User Modified": save it under a name.
    if (s_current == kModifiedName) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
        const bool enter = ImGui::InputText("##ThemeName", s_nameBuffer, sizeof(s_nameBuffer),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        UiProbe::item("settings.theme.name", 0);
        const std::string name = trimmed(s_nameBuffer);
        const std::string problem = nameProblem(name);
        ImGui::SameLine();
        ImGui::BeginDisabled(!problem.empty());
        const bool save = ImGui::Button("Save") || (enter && problem.empty());
        ImGui::EndDisabled();
        UiProbe::item("settings.theme.save", 0);
        if (!problem.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", problem.c_str());
        } else if (name != s_base && findTheme(name)) {
            ImGui::SameLine();
            ImGui::TextDisabled("Replaces the saved theme \"%s\".", name.c_str());
        }
        if (save && problem.empty()) saveModifiedAs(name);
    }
    ImGui::Separator();

    drawStyleEditor(ImGui::GetStyle(), s_baseStyle);
    ImGui::EndChild();

    // Any edit (seen when it ends, not on every frame of a drag) goes to "User Modified".
    if (!ImGui::IsAnyItemActive()) {
        std::vector<SettingsEntry> entries = differencesOf(ImGui::GetStyle());
        if (joined(entries) != s_appliedText) recordEdit(std::move(entries));
    }
}

} // namespace ux3d::imgui
