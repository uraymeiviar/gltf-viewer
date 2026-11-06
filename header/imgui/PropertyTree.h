// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

struct ImVec4;

namespace ux3d::imgui::prop {

// The Inspector's key/value tree: one resizable two-column table (key | value, 50% / 50% by default) whose key
// column is a tree. Groups are collapsible tree nodes (framed at the top level, plain below); property rows put an
// icon and a label on the left (with the description as its tooltip) and the widget, at the value column's full
// width, on the right. Widgets in a value cell use hidden labels ("##name").
//
//   if (prop::begin("SceneInspector")) {
//       if (prop::group(ICON_MDI_EARTH, "Environment")) {
//           prop::row(ICON_MDI_BLUR, "HDR Blur", "Blurs the background.");
//           ImGui::SliderFloat("##hdrBlur", &blur, 0.0f, 1.0f);
//           prop::endGroup();
//       }
//       prop::end();
//   }

// Begins the table (fills the available width). Returns false when it is clipped; call end() only when true.
bool begin(const char* id);
void end();

// True between begin() and end() (for helpers shared with a context outside the Inspector, e.g. a toolbar popup).
bool active();

// A collapsible group row spanning both columns. Returns true when open; then call endGroup() after its rows.
// tooltip (optional) is shown over the row. defaultOpen applies the first time the group is seen.
bool group(const char* icon, const char* label, bool defaultOpen = true, const char* tooltip = nullptr);
void endGroup();

// Like group(), but the group's row keeps its value cell for widgets (an enable switch, a status text): after the call
// the cursor is in the value cell with the item width at the cell's width. Widgets there sit in the group's parent ID
// scope (use unique hidden labels). Call endGroup() when it returns true, as for group().
bool groupValue(const char* icon, const char* label, bool defaultOpen = true, const char* tooltip = nullptr);

// A property row: the key cell (icon + label, tooltip on hover), then moves to the value cell with the item width at
// the cell's width. The next widget(s) go in the value cell. icon may be nullptr.
void row(const char* icon, const char* label, const char* tooltip = nullptr);

// A property row whose value is (disabled) text.
void textRow(const char* icon, const char* label, const char* tooltip, const char* fmt, ...);

// A property row whose value is coloured text (warnings, states).
void colorTextRow(const char* icon, const char* label, const char* tooltip, const ImVec4& color, const char* fmt, ...);

// For a row of buttons in one value cell: before every button but the first, continues on the same line when `width`
// still fits in the cell, else on the next line.
void sameLineOrWrap(float width);

// Sets the tooltip of the last item when it is hovered (also when disabled).
void tooltip(const char* text);

// Width left in the value cell after `reserve` pixels (for a widget followed by a small one on the same line).
float valueWidth(float reserve = 0.0f);

} // namespace ux3d::imgui::prop
