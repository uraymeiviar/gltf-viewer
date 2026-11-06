// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/PropertyTree.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cfloat>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace ux3d::imgui::prop {

namespace {

int s_depth = 0;           // Open groups in the current table.
bool s_widthPushed = false; // An item width is pushed for the current value cell.
bool s_active = false;      // Between begin() and end().
ImGuiID s_pendingPush = 0;  // groupValue(): the open group's tree push, done before the next row.

void flushPush()
{
    if (s_pendingPush != 0) {
        ImGui::TreePushOverrideID(s_pendingPush);
        s_pendingPush = 0;
    }
}

void popWidth()
{
    if (s_widthPushed) {
        ImGui::PopItemWidth();
        s_widthPushed = false;
    }
    flushPush();
}

// A group spans both columns, so the middle of its row is the column border (the resize handle). A probe tag
// (UiProbe::item right after group()) gets the key cell's part of the row, which a click toggles.
void keyCellItemRect()
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    const float cellRight = ImGui::GetCurrentWindow()->WorkRect.Max.x;
    if (g.LastItemData.Rect.Max.x > cellRight) g.LastItemData.Rect.Max.x = cellRight;
}

std::string keyText(const char* icon, const char* label)
{
    std::string text;
    if (icon != nullptr && *icon != '\0') {
        text = icon;
        text += "  ";
    }
    text += label;
    return text;
}

} // namespace

bool begin(const char* id)
{
    s_depth = 0;
    s_widthPushed = false;
    s_pendingPush = 0;
    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_SizingStretchSame;
    if (!ImGui::BeginTable(id, 2, flags)) return false;
    s_active = true;
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
}

void end()
{
    popWidth();
    while (s_depth > 0) endGroup(); // Tolerate an unbalanced caller rather than corrupt the ID stack.
    ImGui::EndTable();
    s_active = false;
}

bool active()
{
    return s_active;
}

bool group(const char* icon, const char* label, bool defaultOpen, const char* tooltip)
{
    popWidth();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    // The label may run into the value column (a group has no value).
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_LabelSpanAllColumns |
                               ImGuiTreeNodeFlags_FramePadding;
    if (s_depth == 0) flags |= ImGuiTreeNodeFlags_Framed;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const std::string text = keyText(icon, label);
    const bool open = ImGui::TreeNodeEx(label, flags, "%s", text.c_str());
    if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tooltip);
    keyCellItemRect();
    if (open) ++s_depth;
    return open;
}

bool groupValue(const char* icon, const char* label, bool defaultOpen, const char* tooltip)
{
    popWidth();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_FramePadding |
                               ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (s_depth == 0) flags |= ImGuiTreeNodeFlags_Framed;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const std::string text = keyText(icon, label);
    const bool open = ImGui::TreeNodeEx(label, flags, "%s", text.c_str());
    if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tooltip);
    // The probe rectangle stays the whole row: a probe tag follows the call in the value cell, whose clip rectangle
    // must overlap it.
    if (open) {
        s_pendingPush = ImGui::GetItemID();
        ++s_depth;
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-FLT_MIN);
    s_widthPushed = true;
    return open;
}

void endGroup()
{
    popWidth();
    if (s_depth > 0) {
        --s_depth;
        ImGui::TreePop();
    }
}

void row(const char* icon, const char* label, const char* tooltip)
{
    popWidth();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    const std::string text = keyText(icon, label);
    // A leaf tree node keeps the key aligned with the group labels at every depth.
    ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                 ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth,
                      "%s", text.c_str());
    if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tooltip);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-FLT_MIN);
    s_widthPushed = true;
}

void textRow(const char* icon, const char* label, const char* tooltip, const char* fmt, ...)
{
    row(icon, label, tooltip);
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", buf);
    if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tooltip);
}

void colorTextRow(const char* icon, const char* label, const char* tooltip, const ImVec4& color, const char* fmt, ...)
{
    row(icon, label, tooltip);
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(color, "%s", buf);
    if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tooltip);
}

void sameLineOrWrap(float width)
{
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < width) ImGui::NewLine();
}

void tooltip(const char* text)
{
    if (text != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
}

float valueWidth(float reserve)
{
    const float w = ImGui::GetContentRegionAvail().x - reserve;
    return w > 1.0f ? w : 1.0f;
}

} // namespace ux3d::imgui::prop
