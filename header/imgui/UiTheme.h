// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

namespace ux3d::imgui {

// The UI theme (File > Settings > UI Theme): every ImGuiStyle color and size.
//
// Themes: "Default" is Glider's built-in theme (applyFssTheme, FontSizeBase = the UI font's size) and is never
// stored. Every other theme lives in settings.ini as [UiTheme.<name>], holding only the values that differ from
// Default (key = ImGuiStyle field name, colors as Color.<ImGuiCol name>; version=1 keeps the section when nothing
// differs). [UiTheme] holds current=<name> (omitted for Default) and, while the current theme is "User Modified",
// base=<the theme it was modified from> (omitted for Default).
//
// Any edit is applied at once and written to the "User Modified" theme (the selected theme's values are copied there
// on the first edit). While "User Modified" is selected, a name field (the base theme's name, or "My Theme" when the
// base is Default) and Save write it as that theme, remove "User Modified" and select the saved theme.
// With persistence disabled (--isolated) nothing is read or written; themes live in memory for the session.
class UiTheme {
public:
    // Applies Default, then the saved current theme. Call once after the fonts are loaded.
    static void apply();

    // Settings window "UI Theme" tab content.
    static void drawSettings();
};

} // namespace ux3d::imgui
